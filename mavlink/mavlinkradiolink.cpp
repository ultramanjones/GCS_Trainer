#include "mavlink/mavlinkradiolink.h"

#include "mavlink/mavlinkframe.h"

#include <QDateTime>
#include <QNetworkDatagram>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtMath>

using namespace MavlinkMessage;

namespace {

// MAVLink sends position as degrees times ten million, in a signed
// thirty two bit integer. That is about one centimeter of resolution
// and it avoids floating point drift on the wire.
constexpr double kDegreesPerRawUnit = 1.0e-7;

// Rough meters per degree. Good enough for a display and for a flight
// of a few kilometers. A real ground station uses a proper geodetic
// library, because these numbers are wrong at high latitude and wrong
// over long distances.
constexpr double kMetersPerDegreeLatitude = 111320.0;

double metersPerDegreeLongitudeAt(double latitudeDegrees)
{
    return kMetersPerDegreeLatitude * std::cos(qDegreesToRadians(latitudeDegrees));
}

} // namespace

MavlinkRadioLink::MavlinkRadioLink(quint16 listenPort,
                                   quint8 groundSystemIdentifier,
                                   QObject *parent)
    : RadioLinkInterface(parent)
    , m_listenPort(listenPort)
    , m_groundSystemIdentifier(groundSystemIdentifier)
{
    m_publishTimer.setInterval(kPublishMilliseconds);
    m_watchdogTimer.setInterval(kWatchdogMilliseconds);
    connect(&m_publishTimer,  &QTimer::timeout, this, &MavlinkRadioLink::publishSitReps);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &MavlinkRadioLink::checkForVehiclesGoneQuiet);
}

MavlinkRadioLink::MavlinkRadioLink(const QString &hostName,
                                   quint16 port,
                                   quint8 groundSystemIdentifier,
                                   QObject *parent)
    : RadioLinkInterface(parent)
    , m_transport(Transport::TcpConnect)
    , m_hostName(hostName)
    , m_listenPort(port)
    , m_groundSystemIdentifier(groundSystemIdentifier)
{
    m_publishTimer.setInterval(kPublishMilliseconds);
    m_watchdogTimer.setInterval(kWatchdogMilliseconds);
    connect(&m_publishTimer,  &QTimer::timeout, this, &MavlinkRadioLink::publishSitReps);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &MavlinkRadioLink::checkForVehiclesGoneQuiet);
}

MavlinkRadioLink::~MavlinkRadioLink() = default;

QString MavlinkRadioLink::radioLinkName() const
{
    if (m_transport == Transport::TcpConnect)
        return QStringLiteral("MAVLink TCP link to %1:%2").arg(m_hostName).arg(m_listenPort);
    return QStringLiteral("MAVLink UDP link on port %1").arg(m_listenPort);
}

int MavlinkRadioLink::badFrameCount() const       { return m_badFrameCount; }
int MavlinkRadioLink::framesReceivedCount() const { return m_framesReceivedCount; }

void MavlinkRadioLink::startListening()
{
    // The socket is built here and not in the constructor. A QObject
    // belongs to the thread it was created on, and this object is
    // built on the main thread and then moved. Build the socket in the
    // constructor and every read happens on the wrong thread.
    if (m_socket || m_streamSocket)
        return;

    if (m_transport == Transport::TcpConnect) {
        m_streamSocket = new QTcpSocket(this);
        connect(m_streamSocket, &QTcpSocket::readyRead,
                this, &MavlinkRadioLink::readPendingStreamBytes);

        // A dial that fails is worth saying out loud. Silence here
        // looks exactly like a vehicle that is powered off.
        connect(m_streamSocket, &QTcpSocket::errorOccurred, this, [this]() {
            qWarning("MavlinkRadioLink could not reach %s:%u: %s",
                     qPrintable(m_hostName), m_listenPort,
                     qPrintable(m_streamSocket->errorString()));
        });

        m_streamSocket->connectToHost(m_hostName, m_listenPort);
        m_publishTimer.start();
        m_watchdogTimer.start();
        return;
    }

    m_socket = new QUdpSocket(this);
    if (!m_socket->bind(QHostAddress::AnyIPv4, m_listenPort, QUdpSocket::ShareAddress)) {
        qWarning("MavlinkRadioLink::startListening could not bind port %u: %s",
                 m_listenPort, qPrintable(m_socket->errorString()));
        delete m_socket;
        m_socket = nullptr;
        return;
    }

    connect(m_socket, &QUdpSocket::readyRead, this, &MavlinkRadioLink::readPendingDatagrams);
    m_publishTimer.start();
    m_watchdogTimer.start();
}

void MavlinkRadioLink::stopListening()
{
    m_publishTimer.stop();
    m_watchdogTimer.stop();
    if (m_socket) {
        m_socket->close();
        delete m_socket;
        m_socket = nullptr;
    }
    if (m_streamSocket) {
        m_streamSocket->close();
        delete m_streamSocket;
        m_streamSocket = nullptr;
    }
    m_receiveBuffer.clear();
}

void MavlinkRadioLink::readPendingDatagrams()
{
    // readyRead fires once even when several datagrams are waiting, so
    // this loops until the socket is empty.
    while (m_socket && m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram datagram = m_socket->receiveDatagram();
        m_receiveBuffer.append(datagram.data());

        // One datagram can hold several frames, and a frame can be split
        // across two datagrams on some links. The buffer handles both.
        drainReceiveBuffer(datagram.senderAddress(), quint16(datagram.senderPort()));
    }
}

// TCP hands us a stream with no message boundaries at all. That costs
// nothing here, because the decoder was already written to find the
// start of a frame in a pile of bytes and to hold a partial frame
// until the rest of it shows up.
void MavlinkRadioLink::readPendingStreamBytes()
{
    if (!m_streamSocket)
        return;

    m_receiveBuffer.append(m_streamSocket->readAll());
    drainReceiveBuffer(m_streamSocket->peerAddress(), quint16(m_streamSocket->peerPort()));
}

void MavlinkRadioLink::drainReceiveBuffer(const QHostAddress &fromAddress, quint16 fromPort)
{
    MavlinkFrame frame;
    while (MavlinkFrameCodec::decodeFirstFrame(m_receiveBuffer, frame, m_badFrameCount)) {
        ++m_framesReceivedCount;
        handleFrame(frame, fromAddress, fromPort);
    }

    // A buffer that only ever grows means we are being fed garbage
    // that never contains a valid frame. Drop it rather than run out
    // of memory over a long flight.
    if (m_receiveBuffer.size() > 64 * 1024)
        m_receiveBuffer.clear();
}

void MavlinkRadioLink::sendFrameBytes(const QByteArray &frameBytes,
                                      const QHostAddress &toAddress,
                                      quint16 toPort)
{
    if (m_transport == Transport::TcpConnect) {
        // The stream already knows where it goes, so the address and
        // port are ignored here.
        if (m_streamSocket && m_streamSocket->state() == QAbstractSocket::ConnectedState)
            m_streamSocket->write(frameBytes);
        return;
    }

    if (m_socket)
        m_socket->writeDatagram(frameBytes, toAddress, toPort);
}

MavlinkRadioLink::VehicleRecord &MavlinkRadioLink::recordFor(int vehicleIdentifier)
{
    auto it = m_vehicleRecords.find(vehicleIdentifier);
    if (it == m_vehicleRecords.end()) {
        VehicleRecord fresh;
        fresh.sitRep.vehicleIdentifier = vehicleIdentifier;
        it = m_vehicleRecords.insert(vehicleIdentifier, fresh);
    }
    return it.value();
}

void MavlinkRadioLink::handleFrame(const MavlinkFrame &frame,
                                   const QHostAddress &fromAddress,
                                   quint16 fromPort)
{
    const int vehicleIdentifier = frame.systemIdentifier;
    VehicleRecord &record = recordFor(vehicleIdentifier);

    // Remember where to send commands. A ground station learns a
    // vehicle's address by hearing from it, not by being configured.
    record.lastSeenAddress = fromAddress;
    record.lastSeenPort = fromPort;
    record.lastHeardMilliseconds = QDateTime::currentMSecsSinceEpoch();

    if (record.hasBeenReportedLost) {
        record.hasBeenReportedLost = false;
        emit contactRegainedWithVehicle(vehicleIdentifier);
    }

    // Sequence numbers count up and wrap at 255. A gap means frames
    // were lost on the way. The operator should be told the link is
    // lossy rather than left to wonder why the display stutters.
    if (record.hasSeenAnySequence) {
        const quint8 expected = quint8(record.lastSequenceNumber + 1);
        if (frame.sequenceNumber != expected) {
            const int missing = quint8(frame.sequenceNumber - expected);
            if (missing > 0 && missing < 128)
                m_badFrameCount += missing;
        }
    }
    record.lastSequenceNumber = frame.sequenceNumber;
    record.hasSeenAnySequence = true;

    switch (frame.messageIdentifier) {

    case kHeartbeat: {
        const Heartbeat message = Heartbeat::unpack(frame.payloadBytes);
        record.sitRep.flightModeName = flightModeName(message.customMode);
        record.sitRep.isArmed = (message.baseMode & Heartbeat::kArmedFlag) != 0;
        break;
    }

    case kSystemStatus: {
        const SystemStatus message = SystemStatus::unpack(frame.payloadBytes);
        record.sitRep.battery.setChargePercent(double(message.batteryRemainingPercent));
        break;
    }

    case kGpsRawInt: {
        const GpsRawInt message = GpsRawInt::unpack(frame.payloadBytes);
        record.sitRep.gpsFixType = message.fixType;
        record.sitRep.satelliteCount = message.satellitesVisible;
        break;
    }

    case kAttitude: {
        const Attitude message = Attitude::unpack(frame.payloadBytes);
        // MAVLink sends angles in radians. People read degrees.
        // Converting here means no layer above ever does trigonometry.
        record.sitRep.attitude.setTargetRollDegrees(qRadiansToDegrees(double(message.rollRadians)));
        record.sitRep.attitude.setTargetPitchDegrees(qRadiansToDegrees(double(message.pitchRadians)));
        record.sitRep.attitude.setHeadingDegrees(qRadiansToDegrees(double(message.yawRadians)));
        record.sitRep.attitude.easeTowardTargets(0.05, 20.0);
        break;
    }

    case kGlobalPositionInt: {
        const GlobalPositionInt message = GlobalPositionInt::unpack(frame.payloadBytes);
        const double latitudeDegrees  = message.latitudeDegreesTimes1e7  * kDegreesPerRawUnit;
        const double longitudeDegrees = message.longitudeDegreesTimes1e7 * kDegreesPerRawUnit;

        // The first fix becomes home. A real ground station takes home
        // from the vehicle's HOME_POSITION message instead of guessing,
        // and this is the obvious next thing to add.
        if (!record.hasHomePosition) {
            record.homeLatitudeDegrees = latitudeDegrees;
            record.homeLongitudeDegrees = longitudeDegrees;
            record.hasHomePosition = true;
        }

        const double northMeters = (latitudeDegrees - record.homeLatitudeDegrees)
                                   * kMetersPerDegreeLatitude;
        const double eastMeters  = (longitudeDegrees - record.homeLongitudeDegrees)
                                   * metersPerDegreeLongitudeAt(record.homeLatitudeDegrees);

        // VehiclePosition holds meters from home and works latitude and
        // longitude out from them. It has no setter that takes meters
        // directly, so the position is rebuilt from home and walked out
        // to where the vehicle is. Adding a setEastNorthMeters to
        // VehiclePosition would make this two lines shorter and is
        // worth doing.
        VehiclePosition position(record.homeLatitudeDegrees, record.homeLongitudeDegrees);
        position.moveAlongHeading(0.0,  northMeters);
        position.moveAlongHeading(90.0, eastMeters);
        position.setAltitudeMetersAboveHome(message.altitudeAboveHomeMillimeters / 1000.0);
        record.sitRep.position = position;

        const double eastMetersPerSecond  = message.velocityEastCentimetersPerSecond  / 100.0;
        const double northMetersPerSecond = message.velocityNorthCentimetersPerSecond / 100.0;
        record.sitRep.groundspeedMetersPerSecond =
            std::hypot(eastMetersPerSecond, northMetersPerSecond);
        break;
    }

    case kVfrHud: {
        const VfrHud message = VfrHud::unpack(frame.payloadBytes);
        record.sitRep.airspeedMetersPerSecond    = double(message.airspeedMetersPerSecond);
        record.sitRep.groundspeedMetersPerSecond = double(message.groundspeedMetersPerSecond);
        break;
    }

    case kCommandAck: {
        const CommandAck message = CommandAck::unpack(frame.payloadBytes);
        const auto waiting = m_commandsAwaitingAnswer.find(message.commandNumber);
        if (waiting == m_commandsAwaitingAnswer.end())
            break;                            // an answer to something we did not send

        VehicleCommandAcknowledgment acknowledgment;
        acknowledgment.vehicleIdentifier = waiting->vehicleIdentifier;
        acknowledgment.requestIdentifier = waiting->requestIdentifier;
        acknowledgment.commandName       = waiting->commandName;
        acknowledgment.wasAccepted       = (message.result == kResultAccepted);

        switch (message.result) {
        case kResultAccepted:            break;
        case kResultTemporarilyRejected: acknowledgment.refusalReason = QStringLiteral("Not right now"); break;
        case kResultDenied:              acknowledgment.refusalReason = QStringLiteral("Refused"); break;
        case kResultUnsupported:         acknowledgment.refusalReason = QStringLiteral("Vehicle does not support this"); break;
        case kResultFailed:              acknowledgment.refusalReason = QStringLiteral("Tried and failed"); break;
        default:                         acknowledgment.refusalReason = QStringLiteral("Refused, reason %1").arg(message.result); break;
        }

        m_commandsAwaitingAnswer.erase(waiting);
        emit vehicleCommandAcknowledged(acknowledgment);
        break;
    }

    case kStatusText:
        // The vehicle talking to the operator in words. Severity 0 to 3
        // is trouble, 4 and 5 are warnings, 6 and above is chatter.
        break;

    default:
        // Unknown message. Ignore it. A real stream carries dozens this
        // program has no use for, and ignoring them is correct.
        break;
    }
}

void MavlinkRadioLink::publishSitReps()
{
    for (auto it = m_vehicleRecords.begin(); it != m_vehicleRecords.end(); ++it) {
        if (it->hasBeenReportedLost)
            continue;                        // nothing new to say about a quiet vehicle
        emit vehicleSitRepReceived(it->sitRep);
    }
}

void MavlinkRadioLink::checkForVehiclesGoneQuiet()
{
    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_vehicleRecords.begin(); it != m_vehicleRecords.end(); ++it) {
        if (it->hasBeenReportedLost)
            continue;
        if (nowMilliseconds - it->lastHeardMilliseconds > kQuietForTooLongMilliseconds) {
            it->hasBeenReportedLost = true;
            emit contactLostWithVehicle(it.key());
        }
    }
}

bool MavlinkRadioLink::buildCommandLong(const QString &commandName,
                                        quint8 targetSystem,
                                        CommandLong &commandOut)
{
    commandOut = CommandLong{};
    commandOut.targetSystem = targetSystem;
    commandOut.targetComponent = 1;          // MAV_COMP_ID_AUTOPILOT1

    if (commandName == VehicleCommandName::Arm) {
        commandOut.commandNumber = kCommandArmDisarm;
        commandOut.parameter1 = 1.0f;
    } else if (commandName == VehicleCommandName::Disarm) {
        commandOut.commandNumber = kCommandArmDisarm;
        commandOut.parameter1 = 0.0f;
    } else if (commandName == VehicleCommandName::Launch) {
        commandOut.commandNumber = kCommandTakeoff;
        commandOut.parameter7 = 60.0f;       // target height above home, meters
    } else if (commandName == VehicleCommandName::ReturnToHome) {
        commandOut.commandNumber = kCommandReturnToLaunch;
    } else if (commandName == VehicleCommandName::Land) {
        commandOut.commandNumber = kCommandLand;
    } else if (commandName == VehicleCommandName::EmergencyStop) {
        commandOut.commandNumber = kCommandFlightTermination;
        commandOut.parameter1 = 1.0f;
    } else {
        return false;
    }
    return true;
}

void MavlinkRadioLink::sendVehicleCommand(VehicleCommandRequest request)
{
    if (!m_socket && !m_streamSocket)
        return;

    const auto found = m_vehicleRecords.constFind(request.vehicleIdentifier);
    if (found == m_vehicleRecords.constEnd() || found->lastSeenPort == 0) {
        // Never heard from this vehicle, so there is nowhere to send.
        // Answering no immediately is better than silence, because the
        // operator is watching a button and waiting.
        VehicleCommandAcknowledgment refusal;
        refusal.vehicleIdentifier = request.vehicleIdentifier;
        refusal.requestIdentifier = request.requestIdentifier;
        refusal.commandName       = request.commandName;
        refusal.wasAccepted       = false;
        refusal.refusalReason     = QStringLiteral("No contact with this vehicle");
        emit vehicleCommandAcknowledged(refusal);
        return;
    }

    CommandLong command;
    if (!buildCommandLong(request.commandName, quint8(request.vehicleIdentifier), command)) {
        VehicleCommandAcknowledgment refusal;
        refusal.vehicleIdentifier = request.vehicleIdentifier;
        refusal.requestIdentifier = request.requestIdentifier;
        refusal.commandName       = request.commandName;
        refusal.wasAccepted       = false;
        refusal.refusalReason     = QStringLiteral("This link cannot send that command");
        emit vehicleCommandAcknowledged(refusal);
        return;
    }

    m_commandsAwaitingAnswer.insert(command.commandNumber, request);

    const QByteArray frameBytes = MavlinkFrameCodec::encodeFrame(
        kCommandLong, command.pack(),
        m_groundSystemIdentifier, 190 /* MAV_COMP_ID_MISSIONPLANNER */,
        m_outgoingSequenceNumber++);

    sendFrameBytes(frameBytes, found->lastSeenAddress, found->lastSeenPort);
}
