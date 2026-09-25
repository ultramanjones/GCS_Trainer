#pragma once

#include <QByteArray>
#include <QString>
#include <QtGlobal>

// The handful of MAVLink messages this program speaks.
//
// A real project generates these from the XML message definitions that
// ship with MAVLink, and ends up with several hundred of them. These
// eight are written by hand because they are the ones a ground station
// actually needs, and because a generated header teaches nothing.
//
// Two rules decide how a payload is laid out, and getting either wrong
// produces a frame that looks fine and decodes to garbage:
//
//   1. Fields are ordered largest first. All the eight byte fields,
//      then the four byte, then the two byte, then the one byte. Not
//      the order they appear in the documentation.
//   2. Everything is little endian, with no padding between fields.
//
// The extra check byte beside each message id is the one the checksum
// routine feeds in at the end. It is computed from the message's field
// list, so it changes when the message changes. If you ever add a field
// here, that byte is wrong until you regenerate it, and every frame
// will fail its check. That is the point of it.
namespace MavlinkMessage {

// Message ids
inline constexpr quint32 kHeartbeat          = 0;
inline constexpr quint32 kSystemStatus       = 1;
inline constexpr quint32 kGpsRawInt          = 24;
inline constexpr quint32 kAttitude           = 30;
inline constexpr quint32 kGlobalPositionInt  = 33;
inline constexpr quint32 kVfrHud             = 74;
inline constexpr quint32 kCommandLong        = 76;
inline constexpr quint32 kCommandAck         = 77;
inline constexpr quint32 kStatusText         = 253;

// The extra check byte for each message above.
quint8 crcExtraForMessage(quint32 messageIdentifier);

// Commands the ground can send, from the MAV_CMD list.
inline constexpr quint16 kCommandArmDisarm      = 400;
inline constexpr quint16 kCommandTakeoff        = 22;
inline constexpr quint16 kCommandReturnToLaunch = 20;
inline constexpr quint16 kCommandLand           = 21;
inline constexpr quint16 kCommandFlightTermination = 185;

// Answers a vehicle can give, from the MAV_RESULT list.
inline constexpr quint8 kResultAccepted            = 0;
inline constexpr quint8 kResultTemporarilyRejected = 1;
inline constexpr quint8 kResultDenied              = 2;
inline constexpr quint8 kResultUnsupported         = 3;
inline constexpr quint8 kResultFailed              = 4;

// Flight modes are custom to each autopilot, so this program keeps its
// own small list and sends it in the custom mode field. A real ground
// station keeps a table per autopilot type and looks the name up.
inline constexpr quint32 kModeStandby   = 0;
inline constexpr quint32 kModeTakeoff   = 1;
inline constexpr quint32 kModeOrbit     = 2;
inline constexpr quint32 kModeReturning = 3;
inline constexpr quint32 kModeLanding   = 4;

QString flightModeName(quint32 customMode);

// ---------------------------------------------------------------------------
// The messages themselves, as plain structs with a pack and an unpack.
// ---------------------------------------------------------------------------

struct Heartbeat
{
    quint32 customMode = 0;
    quint8  vehicleType = 2;       // MAV_TYPE_QUADROTOR
    quint8  autopilotType = 3;     // MAV_AUTOPILOT_ARDUPILOTMEGA
    quint8  baseMode = 0;          // bit 0x80 set means armed
    quint8  systemStatus = 3;      // MAV_STATE_STANDBY
    quint8  mavlinkVersion = 3;

    static constexpr quint8 kArmedFlag = 0x80;

    QByteArray pack() const;
    static Heartbeat unpack(const QByteArray &payload);
};

struct SystemStatus
{
    quint16 batteryMillivolts = 0;
    qint8   batteryRemainingPercent = 0;
    quint16 communicationDropRatePercentTimes100 = 0;

    QByteArray pack() const;
    static SystemStatus unpack(const QByteArray &payload);
};

struct GpsRawInt
{
    quint64 timeMicroseconds = 0;
    qint32  latitudeDegreesTimes1e7 = 0;
    qint32  longitudeDegreesTimes1e7 = 0;
    qint32  altitudeMillimeters = 0;
    quint8  fixType = 0;
    quint8  satellitesVisible = 0;

    QByteArray pack() const;
    static GpsRawInt unpack(const QByteArray &payload);
};

struct Attitude
{
    quint32 timeSinceBootMilliseconds = 0;
    float   rollRadians = 0.0f;
    float   pitchRadians = 0.0f;
    float   yawRadians = 0.0f;
    float   rollRateRadiansPerSecond = 0.0f;
    float   pitchRateRadiansPerSecond = 0.0f;
    float   yawRateRadiansPerSecond = 0.0f;

    QByteArray pack() const;
    static Attitude unpack(const QByteArray &payload);
};

struct GlobalPositionInt
{
    quint32 timeSinceBootMilliseconds = 0;
    qint32  latitudeDegreesTimes1e7 = 0;
    qint32  longitudeDegreesTimes1e7 = 0;
    qint32  altitudeAboveSeaLevelMillimeters = 0;
    qint32  altitudeAboveHomeMillimeters = 0;
    qint16  velocityEastCentimetersPerSecond = 0;
    qint16  velocityNorthCentimetersPerSecond = 0;
    qint16  velocityDownCentimetersPerSecond = 0;
    quint16 headingDegreesTimes100 = 0;

    QByteArray pack() const;
    static GlobalPositionInt unpack(const QByteArray &payload);
};

struct VfrHud
{
    float   airspeedMetersPerSecond = 0.0f;
    float   groundspeedMetersPerSecond = 0.0f;
    float   altitudeMeters = 0.0f;
    float   climbRateMetersPerSecond = 0.0f;
    qint16  headingDegrees = 0;
    quint16 throttlePercent = 0;

    QByteArray pack() const;
    static VfrHud unpack(const QByteArray &payload);
};

struct CommandLong
{
    float   parameter1 = 0.0f;
    float   parameter2 = 0.0f;
    float   parameter3 = 0.0f;
    float   parameter4 = 0.0f;
    float   parameter5 = 0.0f;
    float   parameter6 = 0.0f;
    float   parameter7 = 0.0f;
    quint16 commandNumber = 0;
    quint8  targetSystem = 0;
    quint8  targetComponent = 0;
    quint8  confirmationCount = 0;

    QByteArray pack() const;
    static CommandLong unpack(const QByteArray &payload);
};

struct CommandAck
{
    quint16 commandNumber = 0;
    quint8  result = 0;

    QByteArray pack() const;
    static CommandAck unpack(const QByteArray &payload);
};

struct StatusText
{
    quint8  severity = 6;          // 6 is informational, 3 is an error
    QString text;                  // fifty characters on the wire, no more

    QByteArray pack() const;
    static StatusText unpack(const QByteArray &payload);
};

} // namespace MavlinkMessage
