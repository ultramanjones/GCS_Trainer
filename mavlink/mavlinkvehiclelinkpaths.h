#pragma once

#include <QHostAddress>
#include <QList>
#include <QString>
#include <vector>

#include "mavlink/mavlinklinkpath.h"
#include "model/vehiclealert.h"

// Every path one vehicle is heard on, and which one is the main path.
//
// Why there is a main path at all. The same aircraft can be heard on
// a radio and on a cell modem at the same time. If the ground station
// took state from whichever path spoke last, and sent commands down
// whichever path spoke last, it would flip between them many times a
// second. The cell modem is usually slower, so its copy of a message
// is usually older. Flipping means the screen shows old data and the
// commands go down the slow path.
//
// So one path is the main path:
//   - Commands go out on the main path only. Sending one order down
//     every path can deliver it twice.
//   - Messages that carry no time stamp from the vehicle (heartbeat,
//     battery, airspeed) are taken from the main path only. There is
//     no way to tell which copy is newer, so we listen to one voice.
//   - Messages that DO carry the vehicle's clock (position, attitude,
//     GPS) are taken from any path, newest wins. See
//     MavlinkStaleMessageFilter.
//
// The first path heard becomes the main path. When the main path goes
// quiet and another path is still talking, the main path moves to the
// one heard most recently. It does not move back on its own when the
// old path returns, because switching back and forth is worse than
// staying on a path that works.
//
// A vehicle is out of contact only when EVERY path is quiet. That
// decision is made in MavlinkRadioLink, not here.
//
// Runs on the radio thread only. Nothing here is shared.
class MavlinkVehicleLinkPaths
{
public:
    // Something about the paths the operator should hear about.
    struct LinkPathNotice
    {
        AlertSeverity severity = AlertSeverity::Information;
        QString noticeText;
    };

    // Record a good frame arriving from this address and port. Adds
    // the path the first time it is heard. Returns true when the frame
    // came in on the main path.
    bool noteFrameHeard(const QHostAddress &address, quint16 port,
                        quint8 componentIdentifier, quint8 sequenceNumber,
                        qint64 nowMilliseconds);

    // Called on the watchdog tick. Notes paths that went quiet and
    // moves the main path if it went quiet while another path is
    // still talking.
    void reviewPaths(qint64 nowMilliseconds);

    // The path commands go out on. Null until something is heard.
    const MavlinkLinkPath *mainPath() const;

    const std::vector<MavlinkLinkPath> &allPaths() const;

    // Hands over the notices gathered since the last call, and clears
    // them.
    QList<LinkPathNotice> takeNotices();

private:
    void moveMainPathIfQuiet(qint64 nowMilliseconds);
    int indexOfPath(const QHostAddress &address, quint16 port) const;
    QString roleOfPath(int pathIndex) const;
    void addNotice(AlertSeverity severity, const QString &noticeText);

    std::vector<MavlinkLinkPath> m_paths;
    int m_mainPathIndex = -1;
    QList<LinkPathNotice> m_pendingNotices;
};
