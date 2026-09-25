#pragma once

#include <QByteArray>
#include <QHash>
#include <QHostAddress>
#include <QTimer>

#include "mavlink/mavlinkframe.h"
#include "mavlink/mavlinkmessages.h"
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
// That timer is the coalescing. Messages arrive in the hundreds per
// second across all vehicles. The screen gets twenty updates a second
// carrying the newest of everything. The thread that draws the window
// never sees the difference between a quiet link and a loud one.
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
        QHostAddress lastSeenAddress;
        quint16      lastSeenPort = 0;
        qint64       lastHeardMilliseconds = 0;
        bool         hasBeenReportedLost = false;
        bool         hasHomePosition = false;
        double       homeLatitudeDegrees = 0.0;
        double       homeLongitudeDegrees = 0.0;
        quint8       lastSequenceNumber = 0;
        bool         hasSeenAnySequence = false;
    };

    void handleFrame(const MavlinkFrame &frame,
                     const QHostAddress &fromAddress,
                     quint16 fromPort);

    VehicleRecord &recordFor(int vehicleIdentifier);

    // Pulls whole frames out of the receive buffer and hands them on.
    // Both transports feed the same buffer and call this.
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

    // Commands waiting on an answer, so a late answer can be matched to
    // the order that caused it. Keyed by MAV_CMD number.
    QHash<quint16, VehicleCommandRequest> m_commandsAwaitingAnswer;

    QTimer m_publishTimer;
    QTimer m_watchdogTimer;

    int m_badFrameCount = 0;
    int m_framesReceivedCount = 0;

    static constexpr int kPublishMilliseconds  = 50;    // twenty times a second
    static constexpr int kWatchdogMilliseconds = 250;
    static constexpr qint64 kQuietForTooLongMilliseconds = 1500;
};
