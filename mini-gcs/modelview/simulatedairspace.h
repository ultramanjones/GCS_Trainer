#pragma once

#include <QList>
#include <QObject>
#include <QTimer>
#include <memory>
#include <vector>

#include "model/vehiclecommand.h"
#include "model/vehiclesitrep.h"
#include "modelview/simulatedvehicle.h"

// Everything that is out there.
//
// This is the far end of the radio. It owns the simulated vehicles and
// steps them forward in time. It is the counterpart of the ground
// control station: one owns what is in the air, the other owns what
// the ground believes.
//
// It keeps flying whether or not anybody is listening. Pull the radio
// plug and the aircraft does not stop, which is what actually happens.
class SimulatedAirspace : public QObject
{
    Q_OBJECT

public:
    explicit SimulatedAirspace(QObject *parent = nullptr);
    ~SimulatedAirspace() override;

    // What every vehicle out here would report right now. This is what
    // a radio hears when it listens.
    QList<VehicleSitRep> currentSitReps() const;

    // Hand an order to the vehicle it is addressed to. An order for a
    // vehicle that is not out here comes back refused.
    VehicleCommandAcknowledgment deliverCommand(const VehicleCommandRequest &request);

public slots:
    // Start time moving. Call this only after this object has been
    // moved to its worker thread and that thread is running.
    void startTime();
    void stopTime();

private slots:
    void stepEveryVehicle();

private:
    SimulatedVehicle *vehicleWithIdentifier(int vehicleIdentifier) const;

    std::vector<std::unique_ptr<SimulatedVehicle>> m_vehicles;

    // The timer takes this object as its parent. moveToThread moves an
    // object and its children and nothing else. A timer with no parent
    // stays behind on the thread that built it.
    QTimer m_stepTimer;

    // Fifty steps a second. That is about the rate a real autopilot
    // runs its own loop.
    static constexpr int kStepMilliseconds = 20;
};
