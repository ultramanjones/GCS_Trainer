#include "mavlink/mavlinkstalemessagefilter.h"

bool MavlinkStaleMessageFilter::isNewestSoFar(quint32 messageIdentifier,
                                              quint64 vehicleTimeMicroseconds)
{
    const auto found = m_newestTimeByMessageIdentifier.find(messageIdentifier);

    // The first one of its kind is always the newest so far.
    if (found == m_newestTimeByMessageIdentifier.end()) {
        m_newestTimeByMessageIdentifier.insert(messageIdentifier, vehicleTimeMicroseconds);
        return true;
    }

    const quint64 newestSoFar = found.value();

    if (vehicleTimeMicroseconds > newestSoFar) {
        found.value() = vehicleTimeMicroseconds;
        return true;
    }

    if (vehicleTimeMicroseconds == newestSoFar) {
        ++m_copiesDroppedCount;          // the same report on a second path
        return false;
    }

    // Older than what we have. Either a late arrival, or the vehicle
    // restarted and its clock began again at zero.
    if (newestSoFar - vehicleTimeMicroseconds > kRebootJumpMicroseconds) {
        found.value() = vehicleTimeMicroseconds;
        return true;
    }

    ++m_lateArrivalsDroppedCount;
    return false;
}

int MavlinkStaleMessageFilter::copiesDroppedCount() const       { return m_copiesDroppedCount; }
int MavlinkStaleMessageFilter::lateArrivalsDroppedCount() const { return m_lateArrivalsDroppedCount; }
