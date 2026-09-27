#pragma once

#include <QHash>
#include <QtGlobal>

// Throws away copies and late arrivals of messages that carry the
// vehicle's own clock.
//
// With more than one path, the same position report can arrive twice:
// once on the radio, and a moment later on the cell modem. UDP can
// also deliver two frames out of order on a single path. If the ground
// station simply took whatever arrived last, a late copy would move the
// aircraft backward on the map.
//
// Position, attitude and GPS messages carry a time stamp from the
// vehicle's own clock. This class remembers the newest time stamp seen
// for each kind of message and lets a message through only if it is
// newer. A copy has the same time stamp and is dropped. A late arrival
// has an older one and is dropped.
//
// The ground station's own clock is no use for this. It says when the
// frame arrived, not when the vehicle measured the thing.
//
// One per vehicle. Runs on the radio thread only.
class MavlinkStaleMessageFilter
{
public:
    // True when this message is newer than any of its kind seen so far.
    // The time is in microseconds on the vehicle's clock. Callers with
    // milliseconds multiply by 1000.
    bool isNewestSoFar(quint32 messageIdentifier, quint64 vehicleTimeMicroseconds);

    int copiesDroppedCount() const;
    int lateArrivalsDroppedCount() const;

    // The vehicle's clock starts at zero when it powers up. If a time
    // stamp is this far behind the newest one, the vehicle rebooted.
    // That is not a late message, and it must not freeze the display
    // until the new clock catches up with the old one.
    static constexpr quint64 kRebootJumpMicroseconds = 10'000'000;  // ten seconds

private:
    QHash<quint32, quint64> m_newestTimeByMessageIdentifier;
    int m_copiesDroppedCount = 0;
    int m_lateArrivalsDroppedCount = 0;
};
