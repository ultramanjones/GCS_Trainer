#pragma once

#include <QHash>
#include <QHostAddress>
#include <QString>
#include <QtGlobal>

// One path a vehicle's traffic reaches us on.
//
// A real aircraft often has more than one way to talk to the ground:
// two radios on different channels, and a cell modem. Each of those
// shows up here as a different sending address and port. This class
// is one of them.
//
// It tracks when this path was last heard and how many frames went
// missing on it. Lost frames are counted per path, because a gap on
// the radio says nothing about the cell modem.
//
// Everything here runs on the radio thread. Nothing is shared.
class MavlinkLinkPath
{
public:
    MavlinkLinkPath(const QHostAddress &address, quint16 port);

    const QHostAddress &address() const;
    quint16 port() const;

    // True when this path is the one at this address and port.
    bool isAt(const QHostAddress &address, quint16 port) const;

    // "192.168.1.20:14550", for a log line or an operator message.
    QString description() const;

    // Record one good frame arriving on this path.
    void noteFrameHeard(quint8 componentIdentifier, quint8 sequenceNumber,
                        qint64 nowMilliseconds);

    qint64 lastHeardMilliseconds() const;
    int framesHeardCount() const;
    int framesLostCount() const;

    // Quiet means nothing has arrived on this path for a while. It is
    // not the same as the vehicle being lost. The vehicle may still be
    // talking on another path.
    bool isQuietAt(qint64 nowMilliseconds) const;

    // Whether the operator has already been told this path went quiet,
    // so the same news is not given twice.
    bool hasBeenReportedQuiet() const;
    void setHasBeenReportedQuiet(bool hasBeenReported);

    static constexpr qint64 kQuietAfterMilliseconds = 1000;

private:
    QHostAddress m_address;
    quint16      m_port = 0;

    qint64 m_lastHeardMilliseconds = 0;
    int    m_framesHeardCount = 0;
    int    m_framesLostCount = 0;
    bool   m_hasBeenReportedQuiet = false;

    // MAVLink counts frames separately for every sender. A sender is
    // one component on one vehicle: the autopilot, a camera, a
    // gimbal. So the last number seen is kept per component. Keeping
    // one number per vehicle makes two components look like lost
    // frames.
    QHash<quint8, quint8> m_lastSequenceNumberByComponent;
};
