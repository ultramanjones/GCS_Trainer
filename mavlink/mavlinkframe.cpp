#include "mavlink/mavlinkframe.h"
#include "mavlink/mavlinkmessages.h"

#include <QtEndian>

namespace {

// One step of the MAVLink check value. This is the standard routine
// from the reference implementation, written out rather than pulled in
// so the whole thing is readable in one file.
void accumulateChecksumByte(quint16 &checksum, quint8 byteValue)
{
    quint8 scratch = byteValue ^ static_cast<quint8>(checksum & 0x00FF);
    scratch = static_cast<quint8>(scratch ^ (scratch << 4));
    checksum = static_cast<quint16>((checksum >> 8)
                                    ^ (static_cast<quint16>(scratch) << 8)
                                    ^ (static_cast<quint16>(scratch) << 3)
                                    ^ (static_cast<quint16>(scratch) >> 4));
}

} // namespace

quint16 MavlinkFrameCodec::computeChecksum(const QByteArray &bytes,
                                           int fromIndex,
                                           int byteCount,
                                           quint8 messageCrcExtra)
{
    quint16 checksum = 0xFFFF;
    for (int i = fromIndex; i < fromIndex + byteCount; ++i)
        accumulateChecksumByte(checksum, static_cast<quint8>(bytes.at(i)));
    accumulateChecksumByte(checksum, messageCrcExtra);
    return checksum;
}

QByteArray MavlinkFrameCodec::encodeFrame(quint32 messageIdentifier,
                                          const QByteArray &payloadBytes,
                                          quint8 systemIdentifier,
                                          quint8 componentIdentifier,
                                          quint8 sequenceNumber)
{
    // Version 2 drops trailing zero bytes off the payload before
    // sending. A receiver puts them back by filling the rest of the
    // struct with zeros. It saves radio time on messages where most
    // fields are empty, which is most of them.
    QByteArray trimmedPayload = payloadBytes;
    while (!trimmedPayload.isEmpty() && trimmedPayload.endsWith('\0'))
        trimmedPayload.chop(1);

    QByteArray frameBytes;
    frameBytes.reserve(kHeaderByteCount + trimmedPayload.size() + kChecksumByteCount);

    frameBytes.append(static_cast<char>(kStartMarkerVersion2));
    frameBytes.append(static_cast<char>(trimmedPayload.size()));
    frameBytes.append(static_cast<char>(0));   // incompatible flags, none used here
    frameBytes.append(static_cast<char>(0));   // compatible flags, none used here
    frameBytes.append(static_cast<char>(sequenceNumber));
    frameBytes.append(static_cast<char>(systemIdentifier));
    frameBytes.append(static_cast<char>(componentIdentifier));
    frameBytes.append(static_cast<char>(messageIdentifier & 0xFF));
    frameBytes.append(static_cast<char>((messageIdentifier >> 8) & 0xFF));
    frameBytes.append(static_cast<char>((messageIdentifier >> 16) & 0xFF));
    frameBytes.append(trimmedPayload);

    const quint8 crcExtra = MavlinkMessage::crcExtraForMessage(messageIdentifier);
    const quint16 checksum = computeChecksum(frameBytes, 1, frameBytes.size() - 1, crcExtra);

    frameBytes.append(static_cast<char>(checksum & 0xFF));
    frameBytes.append(static_cast<char>((checksum >> 8) & 0xFF));
    return frameBytes;
}

bool MavlinkFrameCodec::decodeFirstFrame(QByteArray &buffer,
                                         MavlinkFrame &frameOut,
                                         int &badFrameCountInOut)
{
    while (true) {
        // Throw away anything before a start marker. A radio hands you
        // whatever arrived, including the tail of a frame you missed
        // the front of. Either version's marker counts, so take
        // whichever one comes first.
        const int markerTwo = buffer.indexOf(static_cast<char>(kStartMarkerVersion2));
        const int markerOne = buffer.indexOf(static_cast<char>(kStartMarkerVersion1));

        int markerIndex = -1;
        if (markerTwo >= 0 && markerOne >= 0)
            markerIndex = qMin(markerTwo, markerOne);
        else if (markerTwo >= 0)
            markerIndex = markerTwo;
        else
            markerIndex = markerOne;

        if (markerIndex < 0) {
            buffer.clear();
            return false;
        }
        if (markerIndex > 0)
            buffer.remove(0, markerIndex);

        const bool isVersionOne =
            static_cast<quint8>(buffer.at(0)) == kStartMarkerVersion1;
        const int headerSize = isVersionOne ? kHeaderByteCountVersion1
                                            : kHeaderByteCount;

        if (buffer.size() < headerSize)
            return false;                      // header not all here yet

        const int payloadLength = static_cast<quint8>(buffer.at(1));
        const int wholeFrameSize = headerSize + payloadLength + kChecksumByteCount;
        if (buffer.size() < wholeFrameSize)
            return false;                      // payload not all here yet

        // Version 1 puts a single byte message id at offset 5. Version
        // 2 puts three bytes at offsets 7, 8 and 9, low byte first.
        quint32 messageIdentifier = 0;
        if (isVersionOne) {
            messageIdentifier = static_cast<quint8>(buffer.at(5));
        } else {
            messageIdentifier =
                  static_cast<quint32>(static_cast<quint8>(buffer.at(7)))
                | (static_cast<quint32>(static_cast<quint8>(buffer.at(8))) << 8)
                | (static_cast<quint32>(static_cast<quint8>(buffer.at(9))) << 16);
        }

        const quint16 checksumSent =
              static_cast<quint16>(static_cast<quint8>(buffer.at(wholeFrameSize - 2)))
            | (static_cast<quint16>(static_cast<quint8>(buffer.at(wholeFrameSize - 1))) << 8);

        const quint8 crcExtra = MavlinkMessage::crcExtraForMessage(messageIdentifier);
        const quint16 checksumComputed =
            computeChecksum(buffer, 1, headerSize - 1 + payloadLength, crcExtra);

        if (checksumSent != checksumComputed) {
            // Bad frame. Step past this start marker and look for the
            // next one. Do not trust the length byte of a frame that
            // failed its check.
            //
            // A message we have no entry for in the extra check table
            // lands here too, because its extra byte comes back zero
            // and the sum will not match. An autopilot sends dozens of
            // messages we do not care about, so this is normal traffic
            // and not a fault.
            ++badFrameCountInOut;
            buffer.remove(0, 1);
            continue;
        }

        frameOut.sequenceNumber      = static_cast<quint8>(buffer.at(2 + (isVersionOne ? 0 : 2)));
        frameOut.systemIdentifier    = static_cast<quint8>(buffer.at(3 + (isVersionOne ? 0 : 2)));
        frameOut.componentIdentifier = static_cast<quint8>(buffer.at(4 + (isVersionOne ? 0 : 2)));
        frameOut.messageIdentifier   = messageIdentifier;
        frameOut.payloadBytes        = buffer.mid(headerSize, payloadLength);

        buffer.remove(0, wholeFrameSize);
        return true;
    }
}
