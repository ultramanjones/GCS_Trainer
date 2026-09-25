#pragma once

#include <QByteArray>
#include <QtGlobal>

// One MAVLink version 2 frame, taken apart.
//
// A frame is what actually goes over the radio. It is a small header,
// then the payload, then a two byte check value. Everything a vehicle
// says and everything the ground says back is one of these.
//
// The layout, byte by byte:
//
//   0   start marker, always 0xFD for version 2
//   1   payload length, 0 to 255
//   2   incompatible flags
//   3   compatible flags
//   4   sequence number, counts up and wraps at 255
//   5   system id, which vehicle
//   6   component id, which part of that vehicle
//   7-9 message id, three bytes, low byte first
//   10+ payload
//   last two bytes, the check value, low byte first
//
// Version 1 frames start with 0xFE and have a shorter header. This
// code only speaks version 2, which is what anything current uses.
struct MavlinkFrame
{
    quint8  sequenceNumber = 0;
    quint8  systemIdentifier = 0;
    quint8  componentIdentifier = 0;
    quint32 messageIdentifier = 0;
    QByteArray payloadBytes;
};

// Turns frames into bytes and bytes back into frames.
//
// This class holds no Qt object and touches no network. It is pure
// bytes in, bytes out, which means it can be tested with no program
// running and no radio present.
class MavlinkFrameCodec
{
public:
    // Wrap a payload in a frame and hand back the bytes to send.
    //
    // The sequence number is handed in rather than kept here, because
    // the thing that owns the link owns the counting. Two links on one
    // machine each count for themselves.
    static QByteArray encodeFrame(quint32 messageIdentifier,
                                  const QByteArray &payloadBytes,
                                  quint8 systemIdentifier,
                                  quint8 componentIdentifier,
                                  quint8 sequenceNumber);

    // Pull the first whole frame out of a buffer.
    //
    // Returns true when a frame was found and filled in. The bytes it
    // used are removed from the front of the buffer. Returns false when
    // there is not enough yet, and leaves the buffer alone so the
    // caller can try again after more arrives.
    //
    // Garbage in front of a start marker is thrown away. A frame whose
    // check value does not match is thrown away too, and the count of
    // bad frames goes up. A radio in the real world drops and corrupts
    // bytes, so this has to survive both without losing its place.
    static bool decodeFirstFrame(QByteArray &buffer,
                                 MavlinkFrame &frameOut,
                                 int &badFrameCountInOut);

    // The check value MAVLink uses. It is CRC-16/MCRF4XX, run over the
    // frame from the length byte through the end of the payload, and
    // then one extra byte is fed in that depends on the message id.
    //
    // That extra byte is the reason two programs built against
    // different versions of a message definition cannot talk to each
    // other by accident. If the field list changed, the extra byte
    // changed, and every frame fails its check instead of being
    // silently misread. It is a version check disguised as a checksum.
    static quint16 computeChecksum(const QByteArray &bytes,
                                   int fromIndex,
                                   int byteCount,
                                   quint8 messageCrcExtra);

    static constexpr quint8 kStartMarkerVersion2 = 0xFD;
    static constexpr int    kHeaderByteCount     = 10;
    static constexpr int    kChecksumByteCount   = 2;
};
