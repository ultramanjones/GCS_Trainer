#pragma once

#include <QMetaType>
#include <QString>

// What the operator can ask a vehicle to do.
//
// Named, not numbered, because a name is readable in a log at three in
// the morning. A real system would carry MAVLink command numbers here.
namespace VehicleCommandName {
inline const QString Arm             = QStringLiteral("Arm");
inline const QString Disarm          = QStringLiteral("Disarm");
inline const QString Launch          = QStringLiteral("Launch");
inline const QString ReturnToHome    = QStringLiteral("Return");
inline const QString Land            = QStringLiteral("Land");
inline const QString EmergencyStop   = QStringLiteral("EmergencyStop");
}

// An order going out to a vehicle.
//
// The request identifier is what makes an answer matchable to the
// order that caused it. Without it, an answer that arrives late looks
// exactly like an answer to whatever was sent next.
struct VehicleCommandRequest
{
    int vehicleIdentifier = 0;
    int requestIdentifier = 0;
    QString commandName;
};

// The vehicle answering an order. Every order gets one of these.
struct VehicleCommandAcknowledgment
{
    int vehicleIdentifier = 0;
    int requestIdentifier = 0;
    QString commandName;
    bool wasAccepted = false;

    // Filled in when the answer is no. Says what was wrong, in words
    // the operator can read.
    QString refusalReason;
};

Q_DECLARE_METATYPE(VehicleCommandRequest)
Q_DECLARE_METATYPE(VehicleCommandAcknowledgment)
