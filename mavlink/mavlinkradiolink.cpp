#include "mavlink/mavlinkradiolink.h"

#include "mavlink/mavlinkframe.h"

#include <QDateTime>
#include <QList>
#include <QNetworkDatagram>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QtMath>

#include <optional>
#include <utility>

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
    // The timers are made children of this object on purpose. A QTimer
    // that is only a member and not a child does not travel with its
    // owner through moveToThread. It stays on the thread it was made
    // on, and starting it from the worker thread does nothing at all
    // except print "Timers cannot be started from another thread".
    m_publishTimer.setParent(this);
    m_watchdogTimer.setParent(this);
    m_heartbeatTimer.setParent(this);

    m_publishTimer.setInterval(kPublishMilliseconds);
    m_watchdogTimer.setInterval(kWatchdogMilliseconds);
    m_heartbeatTimer.setInterval(kHeartbeatMilliseconds);
    connect(&m_publishTimer,  &QTimer::timeout, this, &MavlinkRadioLink::publishSitReps);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &MavlinkRadioLink::checkForVehiclesGoneQuiet);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &MavlinkRadioLink::sendGroundHeartbeat);
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
    // The timers are made children of this object on purpose. A QTimer
    // that is only a member and not a child does not travel with its
    // owner through moveToThread. It stays on the thread it was made
    // on, and starting it from the worker thread does nothing at all
    // except print "Timers cannot be started from another thread".
    m_publishTimer.setParent(this);
    m_watchdogTimer.setParent(this);
    m_heartbeatTimer.setParent(this);

    m_publishTimer.setInterval(kPublishMilliseconds);
    m_watchdogTimer.setInterval(kWatchdogMilliseconds);
    m_heartbeatTimer.setInterval(kHeartbeatMilliseconds);
    connect(&m_publishTimer,  &QTimer::timeout, this, &MavlinkRadioLink::publishSitReps);
    connect(&m_watchdogTimer, &QTimer::timeout, this, &MavlinkRadioLink::checkForVehiclesGoneQuiet);
    connect(&m_heartbeatTimer, &QTimer::timeout, this, &MavlinkRadioLink::sendGroundHeartbeat);
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

        // A dial that fails must be reported. Silence here
        // looks exactly like a vehicle that is powered off.
        connect(m_streamSocket, &QTcpSocket::errorOccurred, this, [this]() {
            qWarning("MavlinkRadioLink could not reach %s:%u: %s",
                     qPrintable(m_hostName), m_listenPort,
                     qPrintable(m_streamSocket->errorString()));
        });

        m_streamSocket->connectToHost(m_hostName, m_listenPort);
        m_publishTimer.start();
        m_watchdogTimer.start();
        m_heartbeatTimer.start();
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
    m_heartbeatTimer.start();
}

void MavlinkRadioLink::stopListening()
{
    m_publishTimer.stop();
    m_watchdogTimer.stop();
    m_heartbeatTimer.stop();
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


void MavlinkRadioLink::requestTelemetryFrom(quint8 targetSystem,
                                           quint8 targetComponent,
                                           const VehicleRecord &record)
{
    const MavlinkLinkPath *mainPath = record.linkPaths.mainPath();
    if (!mainPath)
        return;

    RequestDataStream request;
    request.targetSystem    = targetSystem;
    request.targetComponent = targetComponent;
    request.streamIdentifier = kDataStreamAll;
    request.requestedRateHertz = 10;
    request.startNotStop = 1;

    sendFrameBytes(MavlinkFrameCodec::encodeFrame(kRequestDataStream,
                                                  request.pack(),
                                                  m_groundSystemIdentifier,
                                                  190,
                                                  m_outgoingSequenceNumber++),
                   mainPath->address(), mainPath->port());
}

// A ground station announces itself once a second, same as a vehicle
// does. Two reasons it matters here. An autopilot watches for the
// ground station going silent and can act on it. And ArduPilot opens a
// connection speaking MAVLink version 1 and only moves up to version 2
// once it has heard a version 2 frame from us - this is that frame.
void MavlinkRadioLink::sendGroundHeartbeat()
{
    Heartbeat beat;
    beat.vehicleType    = 6;    // MAV_TYPE_GCS
    beat.autopilotType  = 8;    // MAV_AUTOPILOT_INVALID, what a GCS sends
    beat.baseMode       = 0;
    beat.customMode     = 0;
    beat.systemStatus   = 4;    // MAV_STATE_ACTIVE
    beat.mavlinkVersion = 3;

    const QByteArray frameBytes =
        MavlinkFrameCodec::encodeFrame(kHeartbeat, beat.pack(),
                                       m_groundSystemIdentifier, 190,
                                       m_outgoingSequenceNumber++);

    if (m_transport == Transport::TcpConnect) {
        sendFrameBytes(frameBytes, QHostAddress(), 0);
        return;
    }

    // On UDP there is nowhere to send until somebody has been heard
    // from. The heartbeat goes out on EVERY path, not just the main
    // one, so the vehicle knows each path still works in both
    // directions.
    for (const VehicleRecord &record : std::as_const(m_vehicleRecords)) {
        for (const MavlinkLinkPath &path : record.linkPaths.allPaths())
            sendFrameBytes(frameBytes, path.address(), path.port());
    }
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

        // Minus one means nobody has told us. A link with no radio in
        // it - a cable, a network socket, a simulator - never sends
        // RADIO_STATUS, and showing zero percent there would read as a
        // dying link instead of a question nobody asked.
        fresh.sitRep.radioSignalPercent = -1;
        it = m_vehicleRecords.insert(vehicleIdentifier, fresh);
    }
    return it.value();
}

void MavlinkRadioLink::handleFrame(const MavlinkFrame &frame,
                                   const QHostAddress &fromAddress,
                                   quint16 fromPort)
{
    // Our own traffic, handed back to us. Links echo: a router in the
    // middle, a radio in loopback, or a simulator forwarding
    // everything to everything. Without this a ground station adds
    // itself to its own vehicle list, then reports itself out of
    // contact the moment it stops talking.
    if (frame.systemIdentifier == m_groundSystemIdentifier)
        return;

    const int vehicleIdentifier = frame.systemIdentifier;
    VehicleRecord &record = recordFor(vehicleIdentifier);
    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();

    // Note which path this frame came in on. A ground station learns a
    // vehicle's paths by hearing from it, not by being configured. The
    // path also counts frames lost on the way, from the sequence
    // number.
    const bool cameOnMainPath =
        record.linkPaths.noteFrameHeard(fromAddress, fromPort,
                                        frame.componentIdentifier,
                                        frame.sequenceNumber,
                                        nowMilliseconds);
    reportLinkPathNotices(vehicleIdentifier, record);

    // Any path counts as contact. The vehicle is lost only when every
    // path is quiet.
    record.lastHeardMilliseconds = nowMilliseconds;

    if (record.hasBeenReportedLost) {
        record.hasBeenReportedLost = false;
        emit contactRegainedWithVehicle(vehicleIdentifier);
    }

    // Which copy of a message to believe, when the same vehicle is
    // heard on more than one path:
    //
    //   - Position, attitude and GPS carry the vehicle's own clock.
    //     They are taken from any path, and the stale message filter
    //     drops copies and late arrivals.
    //   - Everything else carries no clock, so there is no way to tell
    //     which copy is newer. Those are taken from the main path only.
    //     A copy on a backup path still counts as contact, above.
    //   - A command answer is taken from any path. The first copy
    //     matches its order. A second copy finds nothing waiting and is
    //     ignored.
    switch (frame.messageIdentifier) {

    case kHeartbeat: {
        if (!cameOnMainPath)
            break;

        const Heartbeat message = Heartbeat::unpack(frame.payloadBytes);

        // Mode numbers mean nothing on their own. Each autopilot has
        // its own list, and the heartbeat is what says which list to
        // read. Guess wrong and the screen shows a confident, wrong
        // mode name, which is worse than showing none.
        record.sitRep.flightModeName =
            (message.autopilotType == kAutopilotArduPilot)
                ? arduCopterFlightModeName(message.customMode)
                : flightModeName(message.customMode);

        record.sitRep.isArmed = (message.baseMode & Heartbeat::kArmedFlag) != 0;
        record.autopilotType = message.autopilotType;

        // First heartbeat from this vehicle. Ask it to start sending
        // telemetry. A real autopilot says nothing but heartbeats
        // until somebody asks.
        if (!record.hasBeenAskedToStream) {
            record.hasBeenAskedToStream = true;
            requestTelemetryFrom(quint8(vehicleIdentifier),
                                 frame.componentIdentifier, record);
        }
        break;
    }

    case kRadioStatus: {
        // The radio on the ground writes this about its own link, so
        // a copy from another path describes a different radio.
        if (!cameOnMainPath)
            break;

        const RadioStatus message = RadioStatus::unpack(frame.payloadBytes);

        // Report the weaker of the two ends. A link is only as good as
        // its worse direction, and the operator wants the bad news.
        const int weakestRaw = qMin(int(message.localSignalStrength),
                                    int(message.remoteSignalStrength));
        record.sitRep.radioSignalPercent =
            (weakestRaw * 100) / RadioStatus::kFullStrengthRawValue;
        break;
    }

    case kSystemStatus: {
        if (!cameOnMainPath)
            break;

        const SystemStatus message = SystemStatus::unpack(frame.payloadBytes);
        record.sitRep.battery.setChargePercent(double(message.batteryRemainingPercent));
        break;
    }

    case kGpsRawInt: {
        const GpsRawInt message = GpsRawInt::unpack(frame.payloadBytes);
        if (!record.staleMessageFilter.isNewestSoFar(kGpsRawInt, message.timeMicroseconds))
            break;

        record.sitRep.gpsFixType = message.fixType;
        record.sitRep.satelliteCount = message.satellitesVisible;
        break;
    }

    case kAttitude: {
        const Attitude message = Attitude::unpack(frame.payloadBytes);
        if (!record.staleMessageFilter.isNewestSoFar(
                kAttitude, quint64(message.timeSinceBootMilliseconds) * 1000))
            break;

        // MAVLink sends angles in radians. People read degrees.
        // Converting here means no layer above ever does trigonometry.
        record.sitRep.attitude.setTargetRollDegrees(qRadiansToDegrees(double(message.rollRadians)));
        record.sitRep.attitude.setTargetPitchDegrees(qRadiansToDegrees(double(message.pitchRadians)));
        record.sitRep.attitude.setHeadingDegrees(qRadiansToDegrees(double(message.yawRadians)));

        // 0.05 seconds times 20 per second is 1, so this moves all the
        // way to the new angle. The display shows the real value with
        // no smoothing and no lag.
        record.sitRep.attitude.easeTowardTargets(0.05, 20.0);
        break;
    }

    case kGlobalPositionInt: {
        const GlobalPositionInt message = GlobalPositionInt::unpack(frame.payloadBytes);
        if (!record.staleMessageFilter.isNewestSoFar(
                kGlobalPositionInt, quint64(message.timeSinceBootMilliseconds) * 1000))
            break;

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
        if (!cameOnMainPath)
            break;

        const VfrHud message = VfrHud::unpack(frame.payloadBytes);
        record.sitRep.airspeedMetersPerSecond    = double(message.airspeedMetersPerSecond);
        record.sitRep.groundspeedMetersPerSecond = double(message.groundspeedMetersPerSecond);
        break;
    }

    case kCommandAck: {
        const CommandAck message = CommandAck::unpack(frame.payloadBytes);

        const std::optional<MavlinkCommandsAwaitingAnswer::AwaitedCommand> answered =
            m_commandsAwaitingAnswer.takeCommandAnsweredBy(vehicleIdentifier,
                                                           message.commandNumber);
        if (!answered)
            break;          // a second copy, or an answer to something we did not send

        const VehicleCommandRequest &request = answered->request;

        // The answer to the mode change that comes before a launch.
        // Accepted means send the takeoff now. Refused means the
        // launch is over before it started, and the operator hears
        // about it once.
        if (answered->takeoffFollowsModeChange) {
            if (message.result != kResultAccepted) {
                refuseCommand(request, QStringLiteral("Vehicle would not switch to Guided mode"));
                break;
            }

            CommandLong takeoff;
            buildCommandLong(request.commandName, quint8(request.vehicleIdentifier), takeoff);

            MavlinkCommandsAwaitingAnswer::AwaitedCommand awaitedTakeoff;
            awaitedTakeoff.request = request;
            awaitedTakeoff.commandNumber = takeoff.commandNumber;
            awaitedTakeoff.sentAtMilliseconds = nowMilliseconds;
            m_commandsAwaitingAnswer.rememberSentCommand(awaitedTakeoff);

            sendCommandLong(takeoff, record);
            break;
        }

        VehicleCommandAcknowledgment acknowledgment;
        acknowledgment.vehicleIdentifier = request.vehicleIdentifier;
        acknowledgment.requestIdentifier = request.requestIdentifier;
        acknowledgment.commandName       = request.commandName;
        acknowledgment.wasAccepted       = (message.result == kResultAccepted);

        switch (message.result) {
        case kResultAccepted:            break;
        case kResultTemporarilyRejected: acknowledgment.refusalReason = QStringLiteral("Not right now"); break;
        case kResultDenied:              acknowledgment.refusalReason = QStringLiteral("Refused"); break;
        case kResultUnsupported:         acknowledgment.refusalReason = QStringLiteral("Vehicle does not support this"); break;
        case kResultFailed:              acknowledgment.refusalReason = QStringLiteral("Tried and failed"); break;
        default:                         acknowledgment.refusalReason = QStringLiteral("Refused, reason %1").arg(message.result); break;
        }

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

// Runs every 250 ms. Three jobs, all about time running out:
//   1. Paths that went quiet, and moving the main path off a quiet one.
//   2. Vehicles that went quiet on every path.
//   3. Orders that were never answered.
void MavlinkRadioLink::checkForVehiclesGoneQuiet()
{
    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();

    for (auto it = m_vehicleRecords.begin(); it != m_vehicleRecords.end(); ++it) {
        it->linkPaths.reviewPaths(nowMilliseconds);
        reportLinkPathNotices(it.key(), it.value());

        if (it->hasBeenReportedLost)
            continue;
        if (nowMilliseconds - it->lastHeardMilliseconds > kQuietForTooLongMilliseconds) {
            it->hasBeenReportedLost = true;
            emit contactLostWithVehicle(it.key());
        }
    }

    // The ground station has already told the operator "no answer" for
    // these. Nothing more is sent up. They are dropped here so a later
    // order of the same kind is not blocked, and so a very late answer
    // cannot be matched to a newer order.
    const QList<MavlinkCommandsAwaitingAnswer::AwaitedCommand> expired =
        m_commandsAwaitingAnswer.takeExpiredCommands(nowMilliseconds);
    for (const MavlinkCommandsAwaitingAnswer::AwaitedCommand &awaited : expired) {
        qWarning("MavlinkRadioLink::checkForVehiclesGoneQuiet: vehicle %d never answered "
                 "command %u (request %d, \"%s\") within %lld ms - no longer waiting for it",
                 awaited.request.vehicleIdentifier, awaited.commandNumber,
                 awaited.request.requestIdentifier,
                 qPrintable(awaited.request.commandName),
                 MavlinkCommandsAwaitingAnswer::kAnswerWindowMilliseconds);
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
    if (found == m_vehicleRecords.constEnd() || !found->linkPaths.mainPath()) {
        // Never heard from this vehicle, so there is nowhere to send.
        // Answering no immediately is better than silence, because the
        // operator is watching a button and waiting.
        refuseCommand(request, QStringLiteral("No contact with this vehicle"));
        return;
    }

    CommandLong command;
    if (!buildCommandLong(request.commandName, quint8(request.vehicleIdentifier), command)) {
        refuseCommand(request, QStringLiteral("This link cannot send that command"));
        return;
    }

    // Launching an ArduPilot copter takes two orders, not one.
    //
    // A takeoff order is only obeyed in Guided mode, where the
    // autopilot is flying and we are directing it. In the pilot modes
    // a person has the sticks and there is nobody for the order to
    // reach. So the mode change goes out first, and the takeoff waits
    // for its answer.
    //
    // The operator sees none of this. They pressed one button and they
    // get one answer. Which order that answer came from is our
    // bookkeeping, not theirs.
    const bool launchNeedsGuidedModeFirst =
        command.commandNumber == kCommandTakeoff
        && found->autopilotType == kAutopilotArduPilot;

    const quint16 firstCommandNumber =
        launchNeedsGuidedModeFirst ? kCommandDoSetMode : command.commandNumber;

    // One of each command per vehicle at a time. An answer does not say
    // which copy it answers, so two copies waiting at once cannot be
    // told apart. See MavlinkCommandsAwaitingAnswer.
    //
    // Emergency stop is never held back. Cutting the motors twice does
    // the same thing as cutting them once, so a mixed-up answer costs
    // nothing, and a delay could cost the aircraft.
    const bool isEmergencyStop = command.commandNumber == kCommandFlightTermination;
    const bool sameOrderStillWaiting =
        m_commandsAwaitingAnswer.isWaitingFor(request.vehicleIdentifier, firstCommandNumber)
        || m_commandsAwaitingAnswer.isWaitingFor(request.vehicleIdentifier, command.commandNumber);

    if (!isEmergencyStop && sameOrderStillWaiting) {
        refuseCommand(request,
                      QStringLiteral("The vehicle has not answered the last %1 yet. "
                                     "Try again in a few seconds.")
                          .arg(request.commandName));
        return;
    }

    MavlinkCommandsAwaitingAnswer::AwaitedCommand awaited;
    awaited.request = request;
    awaited.commandNumber = firstCommandNumber;
    awaited.sentAtMilliseconds = QDateTime::currentMSecsSinceEpoch();
    awaited.takeoffFollowsModeChange = launchNeedsGuidedModeFirst;
    m_commandsAwaitingAnswer.rememberSentCommand(awaited);

    if (launchNeedsGuidedModeFirst) {
        CommandLong modeChange;
        modeChange.targetSystem    = quint8(request.vehicleIdentifier);
        modeChange.targetComponent = 1;
        modeChange.commandNumber   = kCommandDoSetMode;
        modeChange.parameter1      = float(kBaseModeCustomModeEnabled);
        modeChange.parameter2      = float(kArduCopterModeGuided);
        sendCommandLong(modeChange, *found);
        return;
    }

    sendCommandLong(command, *found);
}

void MavlinkRadioLink::sendCommandLong(const CommandLong &command,
                                       const VehicleRecord &record)
{
    // Orders go out on the main path only. Sending one order down
    // every path can deliver it twice.
    const MavlinkLinkPath *mainPath = record.linkPaths.mainPath();
    if (!mainPath)
        return;

    sendFrameBytes(MavlinkFrameCodec::encodeFrame(
                       kCommandLong, command.pack(),
                       m_groundSystemIdentifier, 190 /* MAV_COMP_ID_MISSIONPLANNER */,
                       m_outgoingSequenceNumber++),
                   mainPath->address(), mainPath->port());
}

void MavlinkRadioLink::refuseCommand(const VehicleCommandRequest &request,
                                     const QString &refusalReason)
{
    VehicleCommandAcknowledgment refusal;
    refusal.vehicleIdentifier = request.vehicleIdentifier;
    refusal.requestIdentifier = request.requestIdentifier;
    refusal.commandName       = request.commandName;
    refusal.wasAccepted       = false;
    refusal.refusalReason     = refusalReason;
    emit vehicleCommandAcknowledged(refusal);
}

void MavlinkRadioLink::reportLinkPathNotices(int vehicleIdentifier, VehicleRecord &record)
{
    const QList<MavlinkVehicleLinkPaths::LinkPathNotice> notices = record.linkPaths.takeNotices();
    for (const MavlinkVehicleLinkPaths::LinkPathNotice &notice : notices)
        emit vehicleLinkPathChanged(vehicleIdentifier, static_cast<int>(notice.severity),
                                    notice.noticeText);
}
