// A check that the MAVLink link handles more than one path to the same
// vehicle, without races.
//
// It sets up the link exactly the way mini-gcs does: the link lives on
// its own worker thread, and everything it reports comes back to the
// main thread through queued signals.
//
// Then it plays the vehicle, talking over two paths at once:
//   the "radio" path - a UDP socket on port 14651
//   the "cell"  path - a UDP socket on port 14652
// Both send to the ground station on port 14650.
//
// Each check prints PASS or FAIL with what it saw. The program exits
// with 0 when everything passes and 1 when anything fails, so a build
// script can run it.
//
// It takes about ten seconds, because some checks wait for paths and
// orders to time out, and those timeouts are real.

#include <QCoreApplication>
#include <QEventLoop>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QThread>
#include <QTimer>
#include <QUdpSocket>

#include <algorithm>
#include <cstdio>

#include "mavlink/mavlinkframe.h"
#include "mavlink/mavlinkmessages.h"
#include "mavlink/mavlinkradiolink.h"
#include "model/vehiclealert.h"

using namespace MavlinkMessage;

namespace {

constexpr quint16 kGroundStationPort = 14650;
constexpr quint16 kRadioPathPort     = 14651;
constexpr quint16 kCellPathPort      = 14652;
constexpr quint8  kVehicleSystemId   = 1;
constexpr quint8  kAutopilotComponentId = 1;
constexpr double  kHomeLatitudeDegrees  = 40.5;
constexpr double  kHomeLongitudeDegrees = -79.8;

int gFailureCount = 0;

void check(bool passed, const char *whatWasChecked, const QString &whatWasSeen)
{
    std::printf("%s  %s\n      saw: %s\n", passed ? "PASS" : "FAIL",
                whatWasChecked, qPrintable(whatWasSeen));
    std::fflush(stdout);
    if (!passed)
        ++gFailureCount;
}

// Let both threads run for a while. The main thread keeps handling its
// events, so queued signals from the link arrive during the wait.
void waitMilliseconds(int milliseconds)
{
    QEventLoop waitLoop;
    QTimer::singleShot(milliseconds, &waitLoop, &QEventLoop::quit);
    waitLoop.exec();
}

// One end of one path, on the vehicle's side. Sends frames to the
// ground station and counts the orders that come back down this path.
class VehiclePathEnd
{
public:
    explicit VehiclePathEnd(quint16 port)
    {
        if (!m_socket.bind(QHostAddress::LocalHost, port)) {
            std::printf("VehiclePathEnd: could not bind port %u: %s\n",
                        port, qPrintable(m_socket.errorString()));
            std::exit(1);
        }
        QObject::connect(&m_socket, &QUdpSocket::readyRead, [this]() { readFromGround(); });
    }

    void sendBytes(const QByteArray &frameBytes)
    {
        m_socket.writeDatagram(frameBytes, QHostAddress::LocalHost, kGroundStationPort);
    }

    int commandsReceivedCount(quint16 commandNumber) const
    {
        return m_commandCounts.value(commandNumber, 0);
    }

private:
    void readFromGround()
    {
        while (m_socket.hasPendingDatagrams()) {
            m_buffer.append(m_socket.receiveDatagram().data());
            MavlinkFrame frame;
            int ignoredBadFrameCount = 0;
            while (MavlinkFrameCodec::decodeFirstFrame(m_buffer, frame, ignoredBadFrameCount)) {
                if (frame.messageIdentifier == kCommandLong) {
                    const CommandLong command = CommandLong::unpack(frame.payloadBytes);
                    m_commandCounts[command.commandNumber] += 1;
                }
            }
        }
    }

    QUdpSocket m_socket;
    QByteArray m_buffer;
    QHash<quint16, int> m_commandCounts;
};

// The vehicle's own frame numbering. One counter for the whole
// vehicle, the same way a real autopilot does it. When both paths carry
// the same frame, they carry the same bytes.
quint8 gVehicleSequenceNumber = 0;

QByteArray vehicleFrame(quint32 messageIdentifier, const QByteArray &payload)
{
    return MavlinkFrameCodec::encodeFrame(messageIdentifier, payload,
                                          kVehicleSystemId, kAutopilotComponentId,
                                          gVehicleSequenceNumber++);
}

QByteArray heartbeatFrame(quint32 arduCopterModeNumber)
{
    Heartbeat beat;
    beat.autopilotType = kAutopilotArduPilot;
    beat.customMode = arduCopterModeNumber;
    return vehicleFrame(kHeartbeat, beat.pack());
}

QByteArray positionFrame(quint32 vehicleTimeMilliseconds, double metersNorthOfHome)
{
    GlobalPositionInt position;
    position.timeSinceBootMilliseconds = vehicleTimeMilliseconds;
    position.latitudeDegreesTimes1e7 =
        qint32((kHomeLatitudeDegrees + metersNorthOfHome / 111320.0) * 1.0e7);
    position.longitudeDegreesTimes1e7 = qint32(kHomeLongitudeDegrees * 1.0e7);
    return vehicleFrame(kGlobalPositionInt, position.pack());
}

QByteArray commandAnswerFrame(quint16 commandNumber, quint8 result)
{
    CommandAck answer;
    answer.commandNumber = commandNumber;
    answer.result = result;
    return vehicleFrame(kCommandAck, answer.pack());
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);

    qRegisterMetaType<VehicleSitRep>();
    qRegisterMetaType<VehicleCommandRequest>();
    qRegisterMetaType<VehicleCommandAcknowledgment>();

    // ---- The link, on its own thread, the same as in mini-gcs ----
    QThread radioThread;
    auto *radioLink = new MavlinkRadioLink(kGroundStationPort);
    radioLink->moveToThread(&radioThread);
    QObject::connect(&radioThread, &QThread::started,
                     radioLink, &RadioLinkInterface::startListening);
    QObject::connect(&radioThread, &QThread::finished,
                     radioLink, &QObject::deleteLater);

    // ---- What the link reports, collected on the main thread ----
    QObject mainThreadCollector;
    VehicleSitRep latestSitRep;
    QList<VehicleCommandAcknowledgment> answers;
    QStringList pathNotices;
    int contactLostCount = 0;

    QObject::connect(radioLink, &RadioLinkInterface::vehicleSitRepReceived,
                     &mainThreadCollector, [&](VehicleSitRep sitRep) { latestSitRep = sitRep; },
                     Qt::QueuedConnection);
    QObject::connect(radioLink, &RadioLinkInterface::vehicleCommandAcknowledged,
                     &mainThreadCollector,
                     [&](VehicleCommandAcknowledgment answer) { answers.append(answer); },
                     Qt::QueuedConnection);
    QObject::connect(radioLink, &RadioLinkInterface::vehicleLinkPathChanged,
                     &mainThreadCollector,
                     [&](int, int, QString noticeText) { pathNotices.append(noticeText); },
                     Qt::QueuedConnection);
    QObject::connect(radioLink, &RadioLinkInterface::contactLostWithVehicle,
                     &mainThreadCollector, [&](int) { ++contactLostCount; },
                     Qt::QueuedConnection);

    radioThread.start();
    waitMilliseconds(200);

    VehiclePathEnd radioPath(kRadioPathPort);
    VehiclePathEnd cellPath(kCellPathPort);

    // Orders go to the link the same way the ground station sends them:
    // queued onto the link's own thread.
    int nextRequestIdentifier = 1;
    auto sendOrder = [&](const QString &commandName) {
        VehicleCommandRequest request;
        request.vehicleIdentifier = kVehicleSystemId;
        request.requestIdentifier = nextRequestIdentifier++;
        request.commandName = commandName;
        QMetaObject::invokeMethod(radioLink,
                                  [radioLink, request]() { radioLink->sendVehicleCommand(request); },
                                  Qt::QueuedConnection);
        return request.requestIdentifier;
    };

    // ---- 1. The same vehicle on two paths is one vehicle ----
    {
        const QByteArray beat = heartbeatFrame(0);        // Stabilize
        radioPath.sendBytes(beat);
        waitMilliseconds(50);
        cellPath.sendBytes(beat);
        const QByteArray firstFix = positionFrame(100000, 0.0);
        radioPath.sendBytes(firstFix);
        cellPath.sendBytes(firstFix);
        waitMilliseconds(250);

        const bool sawBackupNotice = pathNotices.size() == 1
            && pathNotices.first().contains(QString::number(kCellPathPort));
        check(sawBackupNotice,
              "1. A second path to the same vehicle is noted as a backup path",
              pathNotices.join(" | "));
        check(latestSitRep.flightModeName == QLatin1String("Stabilize"),
              "1. The heartbeat is read from the main path",
              latestSitRep.flightModeName);
    }

    // ---- 2. A late copy on the slow path does not move the aircraft back ----
    {
        radioPath.sendBytes(positionFrame(102000, 100.0));   // newer: 100 m north
        waitMilliseconds(50);
        cellPath.sendBytes(positionFrame(101500, 50.0));     // older: 50 m north, arriving late
        waitMilliseconds(250);

        const double northMeters = latestSitRep.position.northMetersFromHome();
        check(qAbs(northMeters - 100.0) < 1.0,
              "2. An older position arriving late on the backup path is dropped",
              QStringLiteral("%1 meters north of home").arg(northMeters, 0, 'f', 1));
    }

    // ---- 3. Messages with no clock are only taken from the main path ----
    {
        cellPath.sendBytes(heartbeatFrame(4));               // Guided, on the backup path
        waitMilliseconds(250);
        check(latestSitRep.flightModeName == QLatin1String("Stabilize"),
              "3. A heartbeat on the backup path does not change the mode",
              latestSitRep.flightModeName);

        radioPath.sendBytes(heartbeatFrame(5));              // Loiter, on the main path
        waitMilliseconds(250);
        check(latestSitRep.flightModeName == QLatin1String("Loiter"),
              "3. A heartbeat on the main path does change the mode",
              latestSitRep.flightModeName);
    }

    // ---- 4. Orders go out on the main path only ----
    const int firstArmRequest = sendOrder(VehicleCommandName::Arm);
    waitMilliseconds(250);
    check(radioPath.commandsReceivedCount(kCommandArmDisarm) == 1
              && cellPath.commandsReceivedCount(kCommandArmDisarm) == 0,
          "4. An order goes out on the main path and not on the backup path",
          QStringLiteral("radio got %1, cell got %2")
              .arg(radioPath.commandsReceivedCount(kCommandArmDisarm))
              .arg(cellPath.commandsReceivedCount(kCommandArmDisarm)));

    // ---- 5. A second Arm is refused while the first is unanswered ----
    {
        const int secondArmRequest = sendOrder(VehicleCommandName::Arm);
        waitMilliseconds(250);
        const bool refusedRightOrder = answers.size() == 1
            && answers.first().requestIdentifier == secondArmRequest
            && !answers.first().wasAccepted;
        check(refusedRightOrder && radioPath.commandsReceivedCount(kCommandArmDisarm) == 1,
              "5. A second Arm is refused, not sent, while the first is waiting",
              answers.isEmpty() ? QStringLiteral("no answer")
                                : answers.first().refusalReason);
        answers.clear();
    }

    // ---- 6. One answer heard on two paths is matched once ----
    {
        const QByteArray answer = commandAnswerFrame(kCommandArmDisarm, kResultAccepted);
        radioPath.sendBytes(answer);
        cellPath.sendBytes(answer);
        waitMilliseconds(250);
        const bool matchedOnce = answers.size() == 1
            && answers.first().requestIdentifier == firstArmRequest
            && answers.first().wasAccepted;
        check(matchedOnce,
              "6. The same answer on two paths is matched to its order exactly once",
              QStringLiteral("%1 answer(s)").arg(answers.size()));
        answers.clear();
    }

    // ---- 7. Emergency stop is never held back ----
    {
        sendOrder(VehicleCommandName::EmergencyStop);
        sendOrder(VehicleCommandName::EmergencyStop);
        waitMilliseconds(250);
        check(radioPath.commandsReceivedCount(kCommandFlightTermination) == 2 && answers.isEmpty(),
              "7. A second emergency stop goes out even while the first is waiting",
              QStringLiteral("%1 sent, %2 refused")
                  .arg(radioPath.commandsReceivedCount(kCommandFlightTermination))
                  .arg(answers.size()));
        answers.clear();
    }

    // ---- 8. A vehicle restart is not mistaken for old data ----
    {
        radioPath.sendBytes(positionFrame(50, 20.0));        // clock back near zero
        waitMilliseconds(250);
        const double northMeters = latestSitRep.position.northMetersFromHome();
        check(qAbs(northMeters - 20.0) < 1.0,
              "8. After a vehicle restart, its new clock is believed",
              QStringLiteral("%1 meters north of home").arg(northMeters, 0, 'f', 1));
    }

    // ---- 9. The main path goes quiet. Commands move. The vehicle is not lost ----
    {
        pathNotices.clear();
        quint32 vehicleClock = 1000;
        for (int step = 0; step < 16; ++step) {             // 1.6 seconds, cell path only
            cellPath.sendBytes(positionFrame(vehicleClock, 30.0));
            vehicleClock += 100;
            waitMilliseconds(100);
        }

        const bool sawSwitch = std::any_of(pathNotices.cbegin(), pathNotices.cend(),
            [](const QString &notice) { return notice.contains(QStringLiteral("Commands now go out on")); });
        check(sawSwitch,
              "9. When the main path goes quiet, commands move to the backup path",
              pathNotices.join(" | "));
        check(contactLostCount == 0,
              "9. The vehicle is NOT reported lost while one path still talks",
              QStringLiteral("contact lost reported %1 time(s)").arg(contactLostCount));

        sendOrder(VehicleCommandName::Land);
        waitMilliseconds(250);
        check(cellPath.commandsReceivedCount(kCommandLand) == 1
                  && radioPath.commandsReceivedCount(kCommandLand) == 0,
              "9. The next order goes out on the new main path",
              QStringLiteral("radio got %1, cell got %2")
                  .arg(radioPath.commandsReceivedCount(kCommandLand))
                  .arg(cellPath.commandsReceivedCount(kCommandLand)));
    }

    // ---- 10. An unanswered order expires and stops blocking ----
    {
        answers.clear();
        waitMilliseconds(int(MavlinkCommandsAwaitingAnswer::kAnswerWindowMilliseconds) + 600);
        sendOrder(VehicleCommandName::Land);
        waitMilliseconds(250);
        check(cellPath.commandsReceivedCount(kCommandLand) == 2 && answers.isEmpty(),
              "10. After the answer window, the same order can be sent again",
              QStringLiteral("Land sent %1 time(s), %2 refusal(s)")
                  .arg(cellPath.commandsReceivedCount(kCommandLand))
                  .arg(answers.size()));
    }

    // ---- 11. Every path quiet means the vehicle is lost, once ----
    //
    // Check 10 already waited long enough with both paths silent, so
    // the vehicle should have been reported lost exactly once.
    check(contactLostCount == 1,
          "11. With every path quiet, the vehicle is reported lost exactly once",
          QStringLiteral("contact lost reported %1 time(s)").arg(contactLostCount));

    radioThread.quit();
    radioThread.wait();

    std::printf("\n%s: %d check(s) failed\n", gFailureCount == 0 ? "ALL PASSED" : "FAILED",
                gFailureCount);
    return gFailureCount == 0 ? 0 : 1;
}
