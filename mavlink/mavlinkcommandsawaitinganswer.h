#pragma once

#include <QHash>
#include <QList>
#include <optional>

#include "model/vehiclecommand.h"

// Commands that have gone out to a vehicle and have not been answered.
//
// The problem this solves. A MAVLink answer (COMMAND_ACK) says which
// command it answers and which vehicle sent it. It does NOT say which
// copy of the command. So if two "Arm" orders to the same vehicle are
// waiting at once, there is no way to know which one an answer belongs
// to. A late answer to the first would be taken as the answer to the
// second.
//
// The rules that close that gap:
//   1. Waiting commands are keyed by vehicle AND command number. Two
//      vehicles can each have an Arm waiting without getting mixed up.
//   2. Only one of each command per vehicle may wait at a time. A
//      second one is refused until the first is answered or expires.
//      Emergency stop is the one exception. See MavlinkRadioLink.
//   3. A waiting command expires after kAnswerWindowMilliseconds. The
//      ground station gives up at 2 seconds. This window is longer on
//      purpose, so an answer that arrives between 2 and 3 seconds is
//      still matched to its own order. The ground station then sees
//      its request number, knows it already gave up on that one, and
//      drops it.
//   4. A second copy of the same answer (heard on a second path) finds
//      nothing waiting and is ignored.
//
// One per radio link. Runs on the radio thread only.
class MavlinkCommandsAwaitingAnswer
{
public:
    struct AwaitedCommand
    {
        VehicleCommandRequest request;
        quint16 commandNumber = 0;
        qint64  sentAtMilliseconds = 0;

        // ArduPilot takes a launch as two orders: switch to Guided
        // mode, then take off. True on the mode change that has a
        // takeoff waiting behind it.
        bool takeoffFollowsModeChange = false;
    };

    bool isWaitingFor(int vehicleIdentifier, quint16 commandNumber) const;

    void rememberSentCommand(const AwaitedCommand &awaitedCommand);

    // Removes and returns the command this answer belongs to, if any
    // is waiting.
    std::optional<AwaitedCommand> takeCommandAnsweredBy(int vehicleIdentifier,
                                                        quint16 commandNumber);

    // Removes and returns every command that has waited too long.
    QList<AwaitedCommand> takeExpiredCommands(qint64 nowMilliseconds);

    static constexpr qint64 kAnswerWindowMilliseconds = 3000;

private:
    // Vehicle id in the high bits, command number in the low sixteen.
    static quint64 keyFor(int vehicleIdentifier, quint16 commandNumber);

    QHash<quint64, AwaitedCommand> m_awaitedCommands;
};
