// A vehicle that speaks real MAVLink.
//
// This is a separate program on purpose. It is the thing on the far
// end of the radio, and keeping it out of the ground station's process
// is the honest arrangement: the only thing the two share is bytes on
// a socket.
//
// Run this, then run mini_gcs with a MavlinkRadioLink, and the ground
// station has no way to tell it apart from a real autopilot except
// that the flight model is simple.
//
// It flies a small profile: sit on the ground, take off when told,
// orbit, come home when told, land. Commands arrive as COMMAND_LONG
// and every one gets a COMMAND_ACK, because a ground station that
// never hears back has to guess, and guessing is what gets people
// hurt.

#include <QCoreApplication>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkDatagram>
#include <QTimer>
#include <QUdpSocket>
#include <QtMath>

#include "mavlink/mavlinkframe.h"
#include "mavlink/mavlinkmessages.h"

using namespace MavlinkMessage;

namespace {
constexpr quint16 kGroundStationPort = 14550;
constexpr quint8  kVehicleSystemId   = 1;
constexpr quint8  kAutopilotComponentId = 1;

constexpr double kHomeLatitudeDegrees  = 40.5065;   // Verona, PA
constexpr double kHomeLongitudeDegrees = -79.8420;
constexpr double kMetersPerDegreeLatitude = 111320.0;

constexpr double kStepSeconds      = 0.02;   // fifty steps a second
constexpr double kOrbitRadiusMeters = 150.0;
constexpr double kCruiseSpeed       = 14.0;  // meters per second
constexpr double kClimbRate         = 3.0;   // meters per second
constexpr double kOrbitAltitude     = 60.0;
} // namespace

class SimulatedMavlinkVehicle : public QObject
{
    Q_OBJECT
public:
    SimulatedMavlinkVehicle()
    {
        m_socket.bind(QHostAddress::AnyIPv4, 14551, QUdpSocket::ShareAddress);
        connect(&m_socket, &QUdpSocket::readyRead, this, &SimulatedMavlinkVehicle::readCommands);

        // Real autopilots send different messages at different rates.
        // Heartbeat is slow and steady, position and attitude are fast.
        connect(&m_flightTimer,    &QTimer::timeout, this, &SimulatedMavlinkVehicle::stepFlight);
        connect(&m_fastTimer,      &QTimer::timeout, this, &SimulatedMavlinkVehicle::sendFastMessages);
        connect(&m_heartbeatTimer, &QTimer::timeout, this, &SimulatedMavlinkVehicle::sendHeartbeat);
        connect(&m_slowTimer,      &QTimer::timeout, this, &SimulatedMavlinkVehicle::sendSlowMessages);

        m_flightTimer.start(int(kStepSeconds * 1000));
        m_fastTimer.start(50);        // twenty a second
        m_heartbeatTimer.start(1000); // one a second, the MAVLink convention
        m_slowTimer.start(500);
        m_bootTime = QDateTime::currentMSecsSinceEpoch();

        qInfo("vehicle up. sending to 127.0.0.1:%u, listening on 14551", kGroundStationPort);
        qInfo("send it Arm, then Launch, then Return from the ground station");
    }

private slots:
    void readCommands()
    {
        while (m_socket.hasPendingDatagrams()) {
            const QNetworkDatagram datagram = m_socket.receiveDatagram();
            m_receiveBuffer.append(datagram.data());

            MavlinkFrame frame;
            int ignoredBadCount = 0;
            while (MavlinkFrameCodec::decodeFirstFrame(m_receiveBuffer, frame, ignoredBadCount)) {
                if (frame.messageIdentifier != kCommandLong)
                    continue;
                handleCommand(CommandLong::unpack(frame.payloadBytes), datagram);
            }
        }
    }

    void handleCommand(const CommandLong &command, const QNetworkDatagram &from)
    {
        quint8 result = kResultAccepted;

        switch (command.commandNumber) {
        case kCommandArmDisarm:
            if (command.parameter1 > 0.5f) {
                if (m_mode != kModeStandby) result = kResultTemporarilyRejected;
                else m_isArmed = true;
            } else {
                if (m_altitudeMeters > 0.5) result = kResultDenied;   // never disarm in flight
                else m_isArmed = false;
            }
            break;

        case kCommandTakeoff:
            if (!m_isArmed)              result = kResultDenied;
            else if (m_mode != kModeStandby) result = kResultTemporarilyRejected;
            else                         m_mode = kModeTakeoff;
            break;

        case kCommandReturnToLaunch:
            if (m_altitudeMeters < 1.0)  result = kResultDenied;
            else                         m_mode = kModeReturning;
            break;

        case kCommandLand:
            if (m_altitudeMeters < 1.0)  result = kResultDenied;
            else                         m_mode = kModeLanding;
            break;

        case kCommandFlightTermination:
            m_mode = kModeLanding;
            m_descendFast = true;
            break;

        default:
            result = kResultUnsupported;
            break;
        }

        CommandAck ack;
        ack.commandNumber = command.commandNumber;
        ack.result = result;
        sendTo(kCommandAck, ack.pack(), from.senderAddress(), quint16(from.senderPort()));

        qInfo("command %u -> %s", command.commandNumber,
              result == kResultAccepted ? "accepted" : "refused");
    }

    void stepFlight()
    {
        switch (m_mode) {
        case kModeStandby:
            break;

        case kModeTakeoff:
            m_altitudeMeters += kClimbRate * kStepSeconds;
            if (m_altitudeMeters >= kOrbitAltitude) {
                m_altitudeMeters = kOrbitAltitude;
                m_mode = kModeOrbit;
            }
            break;

        case kModeOrbit: {
            // Fly a circle of fixed radius around home.
            m_orbitAngleRadians += (kCruiseSpeed / kOrbitRadiusMeters) * kStepSeconds;
            m_eastMeters  = kOrbitRadiusMeters * std::sin(m_orbitAngleRadians);
            m_northMeters = kOrbitRadiusMeters * std::cos(m_orbitAngleRadians);
            m_headingDegrees = std::fmod(qRadiansToDegrees(m_orbitAngleRadians) + 90.0, 360.0);
            m_rollDegrees = 18.0;             // banked into the turn
            break;
        }

        case kModeReturning: {
            // Straight line home, then hand over to landing.
            const double distance = std::hypot(m_eastMeters, m_northMeters);
            if (distance < 2.0) {
                m_mode = kModeLanding;
                m_rollDegrees = 0.0;
                break;
            }
            const double stepMeters = kCruiseSpeed * kStepSeconds;
            m_eastMeters  -= (m_eastMeters  / distance) * stepMeters;
            m_northMeters -= (m_northMeters / distance) * stepMeters;
            m_headingDegrees = std::fmod(qRadiansToDegrees(
                std::atan2(-m_eastMeters, -m_northMeters)) + 360.0, 360.0);
            m_rollDegrees *= 0.95;            // roll out of the bank
            break;
        }

        case kModeLanding:
            m_altitudeMeters -= (m_descendFast ? kClimbRate * 3.0 : kClimbRate) * kStepSeconds;
            if (m_altitudeMeters <= 0.0) {
                m_altitudeMeters = 0.0;
                m_mode = kModeStandby;
                m_isArmed = false;
                m_descendFast = false;
            }
            break;
        }

        // Battery drains whenever the motors are turning.
        if (m_isArmed)
            m_batteryPercent = qMax(0.0, m_batteryPercent - 0.02 * kStepSeconds);
    }

    void sendHeartbeat()
    {
        Heartbeat beat;
        beat.customMode   = m_mode;
        beat.baseMode     = m_isArmed ? Heartbeat::kArmedFlag : 0;
        beat.systemStatus = m_isArmed ? 4 /* ACTIVE */ : 3 /* STANDBY */;
        broadcast(kHeartbeat, beat.pack());
    }

    void sendFastMessages()
    {
        const quint32 sinceBoot = quint32(QDateTime::currentMSecsSinceEpoch() - m_bootTime);

        Attitude attitude;
        attitude.timeSinceBootMilliseconds = sinceBoot;
        attitude.rollRadians  = float(qDegreesToRadians(m_rollDegrees));
        attitude.pitchRadians = float(qDegreesToRadians(m_mode == kModeTakeoff ? 12.0 : 0.0));
        attitude.yawRadians   = float(qDegreesToRadians(m_headingDegrees));
        broadcast(kAttitude, attitude.pack());

        const double latitudeDegrees = kHomeLatitudeDegrees
                                     + m_northMeters / kMetersPerDegreeLatitude;
        const double longitudeDegrees = kHomeLongitudeDegrees
                                      + m_eastMeters / (kMetersPerDegreeLatitude
                                        * std::cos(qDegreesToRadians(kHomeLatitudeDegrees)));

        GlobalPositionInt position;
        position.timeSinceBootMilliseconds = sinceBoot;
        position.latitudeDegreesTimes1e7  = qint32(latitudeDegrees  * 1.0e7);
        position.longitudeDegreesTimes1e7 = qint32(longitudeDegrees * 1.0e7);
        position.altitudeAboveHomeMillimeters = qint32(m_altitudeMeters * 1000.0);
        position.altitudeAboveSeaLevelMillimeters = qint32((m_altitudeMeters + 260.0) * 1000.0);
        position.headingDegreesTimes100 = quint16(m_headingDegrees * 100.0);
        broadcast(kGlobalPositionInt, position.pack());

        VfrHud hud;
        hud.airspeedMetersPerSecond    = float(m_mode == kModeStandby ? 0.0 : kCruiseSpeed);
        hud.groundspeedMetersPerSecond = hud.airspeedMetersPerSecond;
        hud.altitudeMeters             = float(m_altitudeMeters);
        hud.headingDegrees             = qint16(m_headingDegrees);
        hud.throttlePercent            = quint16(m_isArmed ? 55 : 0);
        broadcast(kVfrHud, hud.pack());
    }

    void sendSlowMessages()
    {
        SystemStatus status;
        status.batteryMillivolts = quint16(22200.0 * (m_batteryPercent / 100.0));
        status.batteryRemainingPercent = qint8(m_batteryPercent);
        broadcast(kSystemStatus, status.pack());

        GpsRawInt gps;
        gps.timeMicroseconds = quint64(QDateTime::currentMSecsSinceEpoch()) * 1000ULL;
        gps.fixType = 3;                       // three dimensional fix
        gps.satellitesVisible = 14;
        broadcast(kGpsRawInt, gps.pack());
    }

private:
    void broadcast(quint32 messageIdentifier, const QByteArray &payload)
    {
        sendTo(messageIdentifier, payload, QHostAddress::LocalHost, kGroundStationPort);
    }

    void sendTo(quint32 messageIdentifier, const QByteArray &payload,
                const QHostAddress &address, quint16 port)
    {
        const QByteArray frame = MavlinkFrameCodec::encodeFrame(
            messageIdentifier, payload,
            kVehicleSystemId, kAutopilotComponentId, m_sequenceNumber++);
        m_socket.writeDatagram(frame, address, port);
    }

    QUdpSocket m_socket;
    QByteArray m_receiveBuffer;
    QTimer m_flightTimer, m_fastTimer, m_heartbeatTimer, m_slowTimer;

    quint8  m_sequenceNumber = 0;
    qint64  m_bootTime = 0;

    quint32 m_mode = kModeStandby;
    bool    m_isArmed = false;
    bool    m_descendFast = false;
    double  m_eastMeters = 0.0;
    double  m_northMeters = 0.0;
    double  m_altitudeMeters = 0.0;
    double  m_headingDegrees = 0.0;
    double  m_rollDegrees = 0.0;
    double  m_orbitAngleRadians = 0.0;
    double  m_batteryPercent = 100.0;
};

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    SimulatedMavlinkVehicle vehicle;
    return app.exec();
}

#include "main.moc"
