#include "mavlink/mavlinkcommandsawaitinganswer.h"

bool MavlinkCommandsAwaitingAnswer::isWaitingFor(int vehicleIdentifier,
                                                 quint16 commandNumber) const
{
    return m_awaitedCommands.contains(keyFor(vehicleIdentifier, commandNumber));
}

void MavlinkCommandsAwaitingAnswer::rememberSentCommand(const AwaitedCommand &awaitedCommand)
{
    m_awaitedCommands.insert(keyFor(awaitedCommand.request.vehicleIdentifier,
                                    awaitedCommand.commandNumber),
                             awaitedCommand);
}

std::optional<MavlinkCommandsAwaitingAnswer::AwaitedCommand>
MavlinkCommandsAwaitingAnswer::takeCommandAnsweredBy(int vehicleIdentifier,
                                                     quint16 commandNumber)
{
    const auto found = m_awaitedCommands.find(keyFor(vehicleIdentifier, commandNumber));
    if (found == m_awaitedCommands.end())
        return std::nullopt;

    AwaitedCommand answered = found.value();
    m_awaitedCommands.erase(found);
    return answered;
}

QList<MavlinkCommandsAwaitingAnswer::AwaitedCommand>
MavlinkCommandsAwaitingAnswer::takeExpiredCommands(qint64 nowMilliseconds)
{
    QList<AwaitedCommand> expired;
    for (auto it = m_awaitedCommands.begin(); it != m_awaitedCommands.end();) {
        if (nowMilliseconds - it->sentAtMilliseconds > kAnswerWindowMilliseconds) {
            expired.append(it.value());
            it = m_awaitedCommands.erase(it);
        } else {
            ++it;
        }
    }
    return expired;
}

quint64 MavlinkCommandsAwaitingAnswer::keyFor(int vehicleIdentifier, quint16 commandNumber)
{
    return (quint64(quint32(vehicleIdentifier)) << 16) | commandNumber;
}
