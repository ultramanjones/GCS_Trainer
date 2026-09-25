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
        // the front of.
        const int markerIndex = buffer.indexOf(static_cast<char>(kStartMarkerVersion2));
        if (markerIndex < 0) {
            buffer.clear();
            return false;
        }
        if (markerIndex > 0)
            buffer.remove(0, markerIndex);

        if (buffer.size() < kHeaderByteCount)
            return false;                      // header not all here yet

        const int payloadLength = static_cast<quint8>(buffer.at(1));
        const int wholeFrameSize = kHeaderByteCount + payloadLength + kChecksumByteCount;
        if (buffer.size() < wholeFrameSize)
            return false;                      // payload not all here yet

        const quint32 messageIdentifier =
              static_cast<quint32>(static_cast<quint8>(buffer.at(7)))
            | (static_cast<quint32>(static_cast<quint8>(buffer.at(8))) << 8)
            | (static_cast<quint32>(static_cast<quint8>(buffer.at(9))) << 16);

        const quint16 checksumSent =
              static_cast<quint16>(static_cast<quint8>(buffer.at(wholeFrameSize - 2)))
            | (static_cast<quint16>(static_cast<quint8>(buffer.at(wholeFrameSize - 1))) << 8);

        const quint8 crcExtra = MavlinkMessage::crcExtraForMessage(messageIdentifier);
        const quint16 checksumComputed =
            computeChecksum(buffer, 1, kHeaderByteCount - 1 + payloadLength, crcExtra);

        if (checksumSent != checksumComputed) {
            // Bad frame. Step past this start marker and look for the
            // next one. Do not trust the length byte of a frame that
            // failed its check.
            ++badFrameCountInOut;
            buffer.remove(0, 1);
            continue;
        }

        frameOut.sequenceNumber      = static_cast<quint8>(buffer.at(4));
        frameOut.systemIdentifier    = static_cast<quint8>(buffer.at(5));
        frameOut.componentIdentifier = static_cast<quint8>(buffer.at(6));
        frameOut.messageIdentifier   = messageIdentifier;
        frameOut.payloadBytes        = buffer.mid(kHeaderByteCount, payloadLength);

        buffer.remove(0, wholeFrameSize);
        return true;
    }
}
