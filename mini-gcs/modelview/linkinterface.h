#pragma once

#include <QObject>
#include <QString>

#include "model/telemetrysnapshot.h"

// The shape of any radio link to a vehicle.
//
// Everything above this line talks to a LinkInterface and never to a
// real radio or a fake one. Swap the fake link for a real MAVLink
// radio later and nothing above has to change. QGroundControl has a
// class with this same name and the same job.
//
// All of this runs on a worker thread. Nothing here ever touches the
// screen.
class LinkInterface : public QObject
{
    Q_OBJECT

public:
    explicit LinkInterface(QObject *parent = nullptr) : QObject(parent) {}
    ~LinkInterface() override = default;

    virtual QString linkName() const = 0;

public slots:
    // Start talking to the vehicle. Call this only after the object
    // has been moved to its worker thread and that thread is running.
    virtual void startLink() = 0;

    // Stop talking. Snapshots stop arriving. The screen has to notice
    // on its own that time is passing with no new data.
    virtual void stopLink() = 0;

    // Send one order to the vehicle by name, such as "Arm" or "Land".
    virtual void sendCommand(const QString &commandName) = 0;

signals:
    // The only way vehicle data leaves this object. Always sent from
    // the worker thread.
    void telemetrySnapshotReady(TelemetrySnapshot snapshot);

    // The vehicle answering an order. Every order gets one of these,
    // saying yes or no.
    void commandAcknowledged(QString commandName, bool wasAccepted);

    // Something the vehicle wants the operator to read. The number is
    // an AlertSeverity value.
    void linkMessageReceived(int severityValue, QString messageText);
};
