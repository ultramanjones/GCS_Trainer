#include "mavlink/mavlinkvehiclelinkpaths.h"

#include <utility>

bool MavlinkVehicleLinkPaths::noteFrameHeard(const QHostAddress &address,
                                             quint16 port,
                                             quint8 componentIdentifier,
                                             quint8 sequenceNumber,
                                             qint64 nowMilliseconds)
{
    int pathIndex = indexOfPath(address, port);

    if (pathIndex < 0) {
        m_paths.emplace_back(address, port);
        pathIndex = int(m_paths.size()) - 1;

        if (m_mainPathIndex < 0) {
            // The first path heard. No notice, because the ground
            // station already says the vehicle is on the air.
            m_mainPathIndex = pathIndex;
        } else {
            addNotice(AlertSeverity::Information,
                      QStringLiteral("Also heard on %1. It is now a backup path.")
                          .arg(m_paths[pathIndex].description()));
        }
    }

    MavlinkLinkPath &path = m_paths[pathIndex];
    path.noteFrameHeard(componentIdentifier, sequenceNumber, nowMilliseconds);

    if (path.hasBeenReportedQuiet()) {
        path.setHasBeenReportedQuiet(false);
        addNotice(AlertSeverity::Information,
                  QStringLiteral("%1 path %2 is talking again.")
                      .arg(roleOfPath(pathIndex), path.description()));
    }

    return pathIndex == m_mainPathIndex;
}

void MavlinkVehicleLinkPaths::reviewPaths(qint64 nowMilliseconds)
{
    moveMainPathIfQuiet(nowMilliseconds);

    for (int pathIndex = 0; pathIndex < int(m_paths.size()); ++pathIndex) {
        MavlinkLinkPath &path = m_paths[pathIndex];
        if (path.isQuietAt(nowMilliseconds) && !path.hasBeenReportedQuiet()) {
            path.setHasBeenReportedQuiet(true);
            addNotice(AlertSeverity::Warning,
                      QStringLiteral("%1 path %2 went quiet.")
                          .arg(roleOfPath(pathIndex), path.description()));
        }
    }
}

void MavlinkVehicleLinkPaths::moveMainPathIfQuiet(qint64 nowMilliseconds)
{
    if (m_mainPathIndex < 0 || !m_paths[m_mainPathIndex].isQuietAt(nowMilliseconds))
        return;

    // The main path is quiet. Move to the path heard most recently, as
    // long as that path is not quiet too. If every path is quiet there
    // is nowhere better to go, and the vehicle will be reported lost.
    int bestPathIndex = -1;
    for (int pathIndex = 0; pathIndex < int(m_paths.size()); ++pathIndex) {
        const MavlinkLinkPath &path = m_paths[pathIndex];
        if (path.isQuietAt(nowMilliseconds))
            continue;
        if (bestPathIndex < 0
            || path.lastHeardMilliseconds() > m_paths[bestPathIndex].lastHeardMilliseconds()) {
            bestPathIndex = pathIndex;
        }
    }

    if (bestPathIndex < 0)
        return;

    // One notice covers both facts: the old path went quiet, and the
    // commands moved. Marking the old path as reported keeps the
    // operator from reading the first half twice.
    MavlinkLinkPath &oldMainPath = m_paths[m_mainPathIndex];
    oldMainPath.setHasBeenReportedQuiet(true);
    m_mainPathIndex = bestPathIndex;
    addNotice(AlertSeverity::Warning,
              QStringLiteral("Main path %1 went quiet. Commands now go out on %2.")
                  .arg(oldMainPath.description(), m_paths[bestPathIndex].description()));
}

const MavlinkLinkPath *MavlinkVehicleLinkPaths::mainPath() const
{
    if (m_mainPathIndex < 0)
        return nullptr;
    return &m_paths[m_mainPathIndex];
}

const std::vector<MavlinkLinkPath> &MavlinkVehicleLinkPaths::allPaths() const
{
    return m_paths;
}

QList<MavlinkVehicleLinkPaths::LinkPathNotice> MavlinkVehicleLinkPaths::takeNotices()
{
    return std::exchange(m_pendingNotices, {});
}

int MavlinkVehicleLinkPaths::indexOfPath(const QHostAddress &address, quint16 port) const
{
    for (int pathIndex = 0; pathIndex < int(m_paths.size()); ++pathIndex) {
        if (m_paths[pathIndex].isAt(address, port))
            return pathIndex;
    }
    return -1;
}

QString MavlinkVehicleLinkPaths::roleOfPath(int pathIndex) const
{
    return pathIndex == m_mainPathIndex ? QStringLiteral("Main") : QStringLiteral("Backup");
}

void MavlinkVehicleLinkPaths::addNotice(AlertSeverity severity, const QString &noticeText)
{
    m_pendingNotices.append({ severity, noticeText });
}
