#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QTimer>

#include "mavlink/mavlinkcommandsawaitinganswer.h"
#include "mavlink/mavlinkframe.h"
#include "mavlink/mavlinkmessages.h"
#include "mavlink/mavlinkstalemessagefilter.h"
#include "mavlink/mavlinkvehiclelinkpaths.h"
#include "modelview/radiolinkinterface.h"

class QUdpSocket;
class QTcpSocket;

// A radio whose far end is a real MAVLink stream.
//
// It speaks two ways of moving the same bytes:
//
//   UDP  - bind a port and wait. This is what a real radio, a wifi
//          telemetry bridge, and most simulators do.
//   TCP  - dial out to a host and port. This is what Mission Planner's
//          built in simulator offers on Windows, and what a lot of
//          companion computers expose.
//
// The bytes are identical either way. Only the plumbing differs, so
// the frame decoder and everything above it do not know or care which
// one is in use.
//
// This is the same seam SimulatedRadioLink sits in. Swapping one for
// the other is one line at the composition root, and nothing in the
// view or the viewmodel changes, because all either of them ever sees
// is a VehicleSitRep arriving.
//
// Everything here runs on a worker thread. Nothing here touches the
// screen.
//
// Why a sitrep is built up over several messages rather than one:
//
// MAVLink does not send one message with everything in it. Position
// comes in GLOBAL_POSITION_INT, attitude in ATTITUDE, battery in
// SYS_STATUS, mode and armed state in HEARTBEAT, fix quality in
// GPS_RAW_INT. Each arrives at its own rate. So this class keeps one
// running picture per vehicle, updates whatever piece just arrived,
// and sends the whole picture up on a timer.
//
// That timer is the rate limit. Messages arrive in the hundreds per
// second across all vehicles. The screen gets twenty updates a second
// carrying the newest of everything. The thread that draws the window
// never sees the difference between a quiet link and a loud one.
//
// More than one path to the same vehicle:
//
// A real aircraft is often heard on two radios and a cell modem at
// once. All of them can forward to this one UDP port, each from its own
// address. Three classes keep that from turning into a mess:
//   MavlinkVehicleLinkPaths    - which paths a vehicle is heard on, and
//                                which one is the main path
//   MavlinkStaleMessageFilter  - drops copies and late arrivals, using
//                                the vehicle's own clock
//   MavlinkCommandsAwaitingAnswer - matches each answer to the one
//                                order it belongs to
// A vehicle is out of contact only when every path is quiet.
class MavlinkRadioLink : public RadioLinkInterface
{
    Q_OBJECT

public:
    // listenPort is where vehicle traffic arrives. The address a
    // vehicle is replying to is learned from the first frame it sends,
    // which is how a real ground station finds a vehicle it was not
    // told about.
    explicit MavlinkRadioLink(quint16 listenPort,
                              quint8 groundSystemIdentifier = 255,
                              QObject *parent = nullptr);

    // The TCP form. hostName and port are what to dial, for example
    // "127.0.0.1" and 5762 for Mission Planner's simulator.
    MavlinkRadioLink(const QString &hostName,
                     quint16 port,
                     quint8 groundSystemIdentifier = 255,
                     QObject *parent = nullptr);
    ~MavlinkRadioLink() override;

    QString radioLinkName() const override;

    // Counts for the operator. A link that is quietly dropping frames
    // is worth showing, not hiding.
    int badFrameCount() const;
    int framesReceivedCount() const;

public slots:
    void startListening() override;
    void stopListening() override;
    void sendVehicleCommand(VehicleCommandRequest request) override;

private slots:
    void readPendingDatagrams();
    void readPendingStreamBytes();
    void publishSitReps();
    void checkForVehiclesGoneQuiet();

private:
    // Everything known about one vehicle, kept between messages.
    struct VehicleRecord
    {
        VehicleSitRep sitRep;
        MavlinkVehicleLinkPaths linkPaths;
        MavlinkStaleMessageFilter staleMessageFilter;

        // Last time a frame arrived on ANY path. The vehicle is out of
        // contact only when this goes stale.
        qint64       lastHeardMilliseconds = 0;
        bool         hasBeenReportedLost = false;
        bool         hasHomePosition = false;
        double       homeLatitudeDegrees = 0.0;
        double       homeLongitudeDegrees = 0.0;
        bool         hasBeenAskedToStream = false;
        quint8       autopilotType = 0;
    };

    void handleFrame(const MavlinkFrame &frame,
                     const QHostAddress &fromAddress,
                     quint16 fromPort);

    VehicleRecord &recordFor(int vehicleIdentifier);

    // Pulls whole frames out of the receive buffer and hands them on.
    // Both transports feed the same buffer and call this.
    // Tells a vehicle to start sending telemetry, and how often.
    void requestTelemetryFrom(quint8 targetSystem,
                              quint8 targetComponent,
                              const VehicleRecord &record);

    void sendGroundHeartbeat();

    // Packs a CommandLong and puts it on the wire to one vehicle, on
    // that vehicle's main path.
    void sendCommandLong(const MavlinkMessage::CommandLong &command,
                         const VehicleRecord &record);

    // Answers an order with no, without sending anything.
    void refuseCommand(const VehicleCommandRequest &request, const QString &refusalReason);

    // Passes any path news for this vehicle up to the ground station.
    void reportLinkPathNotices(int vehicleIdentifier, VehicleRecord &record);

    void drainReceiveBuffer(const QHostAddress &fromAddress, quint16 fromPort);

    // Writes finished bytes to whichever socket is open.
    void sendFrameBytes(const QByteArray &frameBytes,
                        const QHostAddress &toAddress,
                        quint16 toPort);

    // Turns a command name from the operator into the MAV_CMD number
    // and its parameters. Returns false for a name this link does not
    // know how to send, which is better than sending a zero.
    static bool buildCommandLong(const QString &commandName,
                                 quint8 targetSystem,
                                 MavlinkMessage::CommandLong &commandOut);

    // Which plumbing this link is using. Set once, at construction.
    enum class Transport { UdpListen, TcpConnect };

    Transport   m_transport = Transport::UdpListen;
    QUdpSocket *m_socket = nullptr;        // used when m_transport is UdpListen
    QTcpSocket *m_streamSocket = nullptr;  // used when m_transport is TcpConnect
    QString     m_hostName;                // TCP only
    quint16     m_listenPort = 0;
    quint8      m_groundSystemIdentifier = 255;
    quint8      m_outgoingSequenceNumber = 0;

    QByteArray  m_receiveBuffer;
    QHash<int, VehicleRecord> m_vehicleRecords;

    // Orders that have gone out and not been answered yet.
    MavlinkCommandsAwaitingAnswer m_commandsAwaitingAnswer;

    QTimer m_publishTimer;
    QTimer m_watchdogTimer;
    QTimer m_heartbeatTimer;

    // Frames that failed their check value. This also counts messages
    // this program has no check table entry for, which on a real
    // autopilot stream is most of them. Frames LOST on the way are a
    // different number, kept per path in MavlinkLinkPath.
    int m_badFrameCount = 0;
    int m_framesReceivedCount = 0;

    static constexpr int kPublishMilliseconds  = 50;    // twenty times a second
    static constexpr int kWatchdogMilliseconds = 250;
    static constexpr int kHeartbeatMilliseconds = 1000;  // once a second
    static constexpr qint64 kQuietForTooLongMilliseconds = 1500;
};
