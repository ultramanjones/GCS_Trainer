#include "mavlink/mavlinklinkpath.h"

MavlinkLinkPath::MavlinkLinkPath(const QHostAddress &address, quint16 port)
    : m_address(address)
    , m_port(port)
{
}

const QHostAddress &MavlinkLinkPath::address() const { return m_address; }
quint16 MavlinkLinkPath::port() const                { return m_port; }

bool MavlinkLinkPath::isAt(const QHostAddress &address, quint16 port) const
{
    return m_port == port && m_address.isEqual(address);
}

QString MavlinkLinkPath::description() const
{
    return QStringLiteral("%1:%2").arg(m_address.toString()).arg(m_port);
}

void MavlinkLinkPath::noteFrameHeard(quint8 componentIdentifier,
                                     quint8 sequenceNumber,
                                     qint64 nowMilliseconds)
{
    m_lastHeardMilliseconds = nowMilliseconds;
    ++m_framesHeardCount;

    // The sequence number goes up by one per frame and wraps from 255
    // back to 0. A jump forward means frames were lost on the way.
    //
    // The subtraction is done in eight bits on purpose, so the wrap
    // from 255 to 0 counts as a step of one. A jump of 128 or more is
    // treated as the sender restarting, not as a loss. That is the
    // same cutoff the MAVLink reference code uses.
    const auto found = m_lastSequenceNumberByComponent.constFind(componentIdentifier);
    if (found != m_lastSequenceNumberByComponent.constEnd()) {
        const quint8 expected = quint8(found.value() + 1);
        const int missing = quint8(sequenceNumber - expected);
        if (missing > 0 && missing < 128)
            m_framesLostCount += missing;
    }
    m_lastSequenceNumberByComponent.insert(componentIdentifier, sequenceNumber);
}

qint64 MavlinkLinkPath::lastHeardMilliseconds() const { return m_lastHeardMilliseconds; }
int MavlinkLinkPath::framesHeardCount() const         { return m_framesHeardCount; }
int MavlinkLinkPath::framesLostCount() const          { return m_framesLostCount; }

bool MavlinkLinkPath::isQuietAt(qint64 nowMilliseconds) const
{
    return nowMilliseconds - m_lastHeardMilliseconds > kQuietAfterMilliseconds;
}

bool MavlinkLinkPath::hasBeenReportedQuiet() const { return m_hasBeenReportedQuiet; }

void MavlinkLinkPath::setHasBeenReportedQuiet(bool hasBeenReported)
{
    m_hasBeenReportedQuiet = hasBeenReported;
}
