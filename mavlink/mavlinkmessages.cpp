#include "mavlink/mavlinkmessages.h"

#include <QtEndian>
#include <cstring>

namespace {

// Small helpers so every pack below reads the same way and nobody has
// to remember which endian call goes with which size.
template <typename T>
void appendLittleEndian(QByteArray &bytes, T value)
{
    char scratch[sizeof(T)];
    qToLittleEndian<T>(value, scratch);
    bytes.append(scratch, sizeof(T));
}

void appendFloat(QByteArray &bytes, float value)
{
    quint32 asBits = 0;
    std::memcpy(&asBits, &value, sizeof(float));
    appendLittleEndian<quint32>(bytes, asBits);
}

template <typename T>
T readLittleEndian(const QByteArray &bytes, int offset)
{
    if (offset + int(sizeof(T)) > bytes.size())
        return T{};                       // short payload, treat the rest as zero
    return qFromLittleEndian<T>(reinterpret_cast<const uchar *>(bytes.constData()) + offset);
}

float readFloat(const QByteArray &bytes, int offset)
{
    const quint32 asBits = readLittleEndian<quint32>(bytes, offset);
    float value = 0.0f;
    std::memcpy(&value, &asBits, sizeof(float));
    return value;
}

} // namespace

namespace MavlinkMessage {

quint8 crcExtraForMessage(quint32 messageIdentifier)
{
    // These come from the MAVLink common dialect. If you ever point
    // this code at a real autopilot and every frame fails its check,
    // this table is the first place to look.
    switch (messageIdentifier) {
    case kHeartbeat:         return 50;
    case kSystemStatus:      return 124;
    case kGpsRawInt:         return 24;
    case kAttitude:          return 39;
    case kGlobalPositionInt: return 104;
    case kVfrHud:            return 20;
    case kCommandLong:       return 152;
    case kCommandAck:        return 143;
    case kStatusText:        return 83;
    case kRequestDataStream: return 148;
    case kRadioStatus:       return 185;
    default:                 return 0;
    }
}

RadioStatus RadioStatus::unpack(const QByteArray &payload)
{
    RadioStatus m;
    m.receiveErrorCount     = readLittleEndian<quint16>(payload, 0);
    m.correctedPacketCount  = readLittleEndian<quint16>(payload, 2);
    m.localSignalStrength   = payload.size() > 4 ? quint8(payload.at(4)) : 0;
    m.remoteSignalStrength  = payload.size() > 5 ? quint8(payload.at(5)) : 0;
    m.transmitBufferPercent = payload.size() > 6 ? quint8(payload.at(6)) : 0;
    m.localNoise            = payload.size() > 7 ? quint8(payload.at(7)) : 0;
    m.remoteNoise           = payload.size() > 8 ? quint8(payload.at(8)) : 0;
    return m;
}

QByteArray RequestDataStream::pack() const
{
    QByteArray p;
    appendLittleEndian<quint16>(p, requestedRateHertz);
    p.append(static_cast<char>(targetSystem));
    p.append(static_cast<char>(targetComponent));
    p.append(static_cast<char>(streamIdentifier));
    p.append(static_cast<char>(startNotStop));
    return p;
}

QString arduCopterFlightModeName(quint32 customMode)
{
    switch (customMode) {
    case 0:  return QStringLiteral("Stabilize");
    case 1:  return QStringLiteral("Acro");
    case 2:  return QStringLiteral("AltHold");
    case 3:  return QStringLiteral("Auto");
    case 4:  return QStringLiteral("Guided");
    case 5:  return QStringLiteral("Loiter");
    case 6:  return QStringLiteral("RTL");
    case 7:  return QStringLiteral("Circle");
    case 9:  return QStringLiteral("Land");
    case 11: return QStringLiteral("Drift");
    case 13: return QStringLiteral("Sport");
    case 14: return QStringLiteral("Flip");
    case 15: return QStringLiteral("AutoTune");
    case 16: return QStringLiteral("PosHold");
    case 17: return QStringLiteral("Brake");
    case 18: return QStringLiteral("Throw");
    case 20: return QStringLiteral("Guided NoGPS");
    case 21: return QStringLiteral("Smart RTL");
    case 25: return QStringLiteral("SystemID");
    case 27: return QStringLiteral("Auto RTL");
    default: return QStringLiteral("Mode %1").arg(customMode);
    }
}

QString flightModeName(quint32 customMode)
{
    switch (customMode) {
    case kModeStandby:   return QStringLiteral("Standby");
    case kModeTakeoff:   return QStringLiteral("Takeoff");
    case kModeOrbit:     return QStringLiteral("Orbit");
    case kModeReturning: return QStringLiteral("Returning");
    case kModeLanding:   return QStringLiteral("Landing");
    default:             return QStringLiteral("Unknown");
    }
}

// --- Heartbeat -------------------------------------------------------------
// Four byte field first, then the five single bytes.
QByteArray Heartbeat::pack() const
{
    QByteArray p;
    appendLittleEndian<quint32>(p, customMode);
    p.append(static_cast<char>(vehicleType));
    p.append(static_cast<char>(autopilotType));
    p.append(static_cast<char>(baseMode));
    p.append(static_cast<char>(systemStatus));
    p.append(static_cast<char>(mavlinkVersion));
    return p;
}

Heartbeat Heartbeat::unpack(const QByteArray &payload)
{
    Heartbeat m;
    m.customMode     = readLittleEndian<quint32>(payload, 0);
    m.vehicleType    = payload.size() > 4 ? quint8(payload.at(4)) : 0;
    m.autopilotType  = payload.size() > 5 ? quint8(payload.at(5)) : 0;
    m.baseMode       = payload.size() > 6 ? quint8(payload.at(6)) : 0;
    m.systemStatus   = payload.size() > 7 ? quint8(payload.at(7)) : 0;
    m.mavlinkVersion = payload.size() > 8 ? quint8(payload.at(8)) : 0;
    return m;
}

// --- SystemStatus ----------------------------------------------------------
// The real message carries sensor bitmaps and error counters this
// program has no use for. They are packed as zeros so the field
// positions stay exactly where a real receiver expects them.
QByteArray SystemStatus::pack() const
{
    QByteArray p;
    appendLittleEndian<quint32>(p, 0);   // sensors present
    appendLittleEndian<quint32>(p, 0);   // sensors enabled
    appendLittleEndian<quint32>(p, 0);   // sensors health
    appendLittleEndian<quint16>(p, 0);   // load
    appendLittleEndian<quint16>(p, batteryMillivolts);
    appendLittleEndian<qint16>(p, -1);   // current, -1 means not measured
    appendLittleEndian<quint16>(p, communicationDropRatePercentTimes100);
    appendLittleEndian<quint16>(p, 0);   // comm errors
    appendLittleEndian<quint16>(p, 0);   // error count 1
    appendLittleEndian<quint16>(p, 0);   // error count 2
    appendLittleEndian<quint16>(p, 0);   // error count 3
    appendLittleEndian<quint16>(p, 0);   // error count 4
    p.append(static_cast<char>(batteryRemainingPercent));
    return p;
}

SystemStatus SystemStatus::unpack(const QByteArray &payload)
{
    SystemStatus m;
    m.batteryMillivolts = readLittleEndian<quint16>(payload, 14);
    m.communicationDropRatePercentTimes100 = readLittleEndian<quint16>(payload, 18);
    m.batteryRemainingPercent = payload.size() > 30 ? qint8(payload.at(30)) : 0;
    return m;
}

// --- GpsRawInt -------------------------------------------------------------
QByteArray GpsRawInt::pack() const
{
    QByteArray p;
    appendLittleEndian<quint64>(p, timeMicroseconds);
    appendLittleEndian<qint32>(p, latitudeDegreesTimes1e7);
    appendLittleEndian<qint32>(p, longitudeDegreesTimes1e7);
    appendLittleEndian<qint32>(p, altitudeMillimeters);
    appendLittleEndian<quint16>(p, 65535);   // horizontal accuracy, unknown
    appendLittleEndian<quint16>(p, 65535);   // vertical accuracy, unknown
    appendLittleEndian<quint16>(p, 65535);   // ground speed, unknown
    appendLittleEndian<quint16>(p, 65535);   // course over ground, unknown
    p.append(static_cast<char>(fixType));
    p.append(static_cast<char>(satellitesVisible));
    return p;
}

GpsRawInt GpsRawInt::unpack(const QByteArray &payload)
{
    GpsRawInt m;
    m.timeMicroseconds          = readLittleEndian<quint64>(payload, 0);
    m.latitudeDegreesTimes1e7   = readLittleEndian<qint32>(payload, 8);
    m.longitudeDegreesTimes1e7  = readLittleEndian<qint32>(payload, 12);
    m.altitudeMillimeters       = readLittleEndian<qint32>(payload, 16);
    m.fixType                   = payload.size() > 28 ? quint8(payload.at(28)) : 0;
    m.satellitesVisible         = payload.size() > 29 ? quint8(payload.at(29)) : 0;
    return m;
}

// --- Attitude --------------------------------------------------------------
QByteArray Attitude::pack() const
{
    QByteArray p;
    appendLittleEndian<quint32>(p, timeSinceBootMilliseconds);
    appendFloat(p, rollRadians);
    appendFloat(p, pitchRadians);
    appendFloat(p, yawRadians);
    appendFloat(p, rollRateRadiansPerSecond);
    appendFloat(p, pitchRateRadiansPerSecond);
    appendFloat(p, yawRateRadiansPerSecond);
    return p;
}

Attitude Attitude::unpack(const QByteArray &payload)
{
    Attitude m;
    m.timeSinceBootMilliseconds = readLittleEndian<quint32>(payload, 0);
    m.rollRadians               = readFloat(payload, 4);
    m.pitchRadians              = readFloat(payload, 8);
    m.yawRadians                = readFloat(payload, 12);
    m.rollRateRadiansPerSecond  = readFloat(payload, 16);
    m.pitchRateRadiansPerSecond = readFloat(payload, 20);
    m.yawRateRadiansPerSecond   = readFloat(payload, 24);
    return m;
}

// --- GlobalPositionInt -----------------------------------------------------
QByteArray GlobalPositionInt::pack() const
{
    QByteArray p;
    appendLittleEndian<quint32>(p, timeSinceBootMilliseconds);
    appendLittleEndian<qint32>(p, latitudeDegreesTimes1e7);
    appendLittleEndian<qint32>(p, longitudeDegreesTimes1e7);
    appendLittleEndian<qint32>(p, altitudeAboveSeaLevelMillimeters);
    appendLittleEndian<qint32>(p, altitudeAboveHomeMillimeters);
    appendLittleEndian<qint16>(p, velocityEastCentimetersPerSecond);
    appendLittleEndian<qint16>(p, velocityNorthCentimetersPerSecond);
    appendLittleEndian<qint16>(p, velocityDownCentimetersPerSecond);
    appendLittleEndian<quint16>(p, headingDegreesTimes100);
    return p;
}

GlobalPositionInt GlobalPositionInt::unpack(const QByteArray &payload)
{
    GlobalPositionInt m;
    m.timeSinceBootMilliseconds        = readLittleEndian<quint32>(payload, 0);
    m.latitudeDegreesTimes1e7          = readLittleEndian<qint32>(payload, 4);
    m.longitudeDegreesTimes1e7         = readLittleEndian<qint32>(payload, 8);
    m.altitudeAboveSeaLevelMillimeters = readLittleEndian<qint32>(payload, 12);
    m.altitudeAboveHomeMillimeters     = readLittleEndian<qint32>(payload, 16);
    m.velocityEastCentimetersPerSecond  = readLittleEndian<qint16>(payload, 20);
    m.velocityNorthCentimetersPerSecond = readLittleEndian<qint16>(payload, 22);
    m.velocityDownCentimetersPerSecond  = readLittleEndian<qint16>(payload, 24);
    m.headingDegreesTimes100            = readLittleEndian<quint16>(payload, 26);
    return m;
}

// --- VfrHud ----------------------------------------------------------------
QByteArray VfrHud::pack() const
{
    QByteArray p;
    appendFloat(p, airspeedMetersPerSecond);
    appendFloat(p, groundspeedMetersPerSecond);
    appendFloat(p, altitudeMeters);
    appendFloat(p, climbRateMetersPerSecond);
    appendLittleEndian<qint16>(p, headingDegrees);
    appendLittleEndian<quint16>(p, throttlePercent);
    return p;
}

VfrHud VfrHud::unpack(const QByteArray &payload)
{
    VfrHud m;
    m.airspeedMetersPerSecond    = readFloat(payload, 0);
    m.groundspeedMetersPerSecond = readFloat(payload, 4);
    m.altitudeMeters             = readFloat(payload, 8);
    m.climbRateMetersPerSecond   = readFloat(payload, 12);
    m.headingDegrees             = readLittleEndian<qint16>(payload, 16);
    m.throttlePercent            = readLittleEndian<quint16>(payload, 18);
    return m;
}

// --- CommandLong -----------------------------------------------------------
QByteArray CommandLong::pack() const
{
    QByteArray p;
    appendFloat(p, parameter1);
    appendFloat(p, parameter2);
    appendFloat(p, parameter3);
    appendFloat(p, parameter4);
    appendFloat(p, parameter5);
    appendFloat(p, parameter6);
    appendFloat(p, parameter7);
    appendLittleEndian<quint16>(p, commandNumber);
    p.append(static_cast<char>(targetSystem));
    p.append(static_cast<char>(targetComponent));
    p.append(static_cast<char>(confirmationCount));
    return p;
}

CommandLong CommandLong::unpack(const QByteArray &payload)
{
    CommandLong m;
    m.parameter1 = readFloat(payload, 0);
    m.parameter2 = readFloat(payload, 4);
    m.parameter3 = readFloat(payload, 8);
    m.parameter4 = readFloat(payload, 12);
    m.parameter5 = readFloat(payload, 16);
    m.parameter6 = readFloat(payload, 20);
    m.parameter7 = readFloat(payload, 24);
    m.commandNumber     = readLittleEndian<quint16>(payload, 28);
    m.targetSystem      = payload.size() > 30 ? quint8(payload.at(30)) : 0;
    m.targetComponent   = payload.size() > 31 ? quint8(payload.at(31)) : 0;
    m.confirmationCount = payload.size() > 32 ? quint8(payload.at(32)) : 0;
    return m;
}

// --- CommandAck ------------------------------------------------------------
QByteArray CommandAck::pack() const
{
    QByteArray p;
    appendLittleEndian<quint16>(p, commandNumber);
    p.append(static_cast<char>(result));
    return p;
}

CommandAck CommandAck::unpack(const QByteArray &payload)
{
    CommandAck m;
    m.commandNumber = readLittleEndian<quint16>(payload, 0);
    m.result        = payload.size() > 2 ? quint8(payload.at(2)) : 0;
    return m;
}

// --- StatusText ------------------------------------------------------------
QByteArray StatusText::pack() const
{
    QByteArray p;
    p.append(static_cast<char>(severity));
    QByteArray textBytes = text.toUtf8().left(50);
    textBytes.resize(50);           // pad with zeros, the wire size is fixed
    p.append(textBytes);
    return p;
}

StatusText StatusText::unpack(const QByteArray &payload)
{
    StatusText m;
    m.severity = payload.isEmpty() ? 6 : quint8(payload.at(0));
    const QByteArray textBytes = payload.mid(1, 50);
    m.text = QString::fromUtf8(textBytes.left(qMax(0, int(qstrnlen(textBytes.constData(),
                                                                   textBytes.size())))));
    return m;
}

} // namespace MavlinkMessage
