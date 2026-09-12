#pragma once

#include <QObject>
#include <QString>

#include "model/vehiclecommand.h"
#include "model/vehiclesitrep.h"

// The shape of any radio link to a vehicle.
//
// This is the seam. Everything on the ground talks to a
// RadioLinkInterface and never to a particular radio. Swap the
// simulated link for a real MAVLink radio at the composition root, one
// line, and nothing above changes, because all the ground station ever
// sees is messages arriving.
//
// The seam belongs here and not at the vehicle. A ground station never
// holds the aircraft. It holds what the radio told it.
//
// QGroundControl calls this LinkInterface.
class RadioLinkInterface : public QObject
{
    Q_OBJECT

public:
    explicit RadioLinkInterface(QObject *parent = nullptr) : QObject(parent) {}
    ~RadioLinkInterface() override = default;

    virtual QString radioLinkName() const = 0;

public slots:
    // Start and stop hearing traffic. These do nothing to the
    // vehicles. Unplugging a radio does not land an aircraft.
    virtual void startListening() = 0;
    virtual void stopListening() = 0;

    virtual void sendVehicleCommand(VehicleCommandRequest request) = 0;

signals:
    void vehicleSitRepReceived(VehicleSitRep sitRep);
    void vehicleCommandAcknowledged(VehicleCommandAcknowledgment acknowledgment);

    // The radio is the first thing to know that a vehicle has gone
    // quiet, so the radio is what notices. It reports which vehicle,
    // because one radio can carry several and losing one does not mean
    // losing them all.
    void contactLostWithVehicle(int vehicleIdentifier);
    void contactRegainedWithVehicle(int vehicleIdentifier);
};
