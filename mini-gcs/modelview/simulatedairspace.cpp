#include "modelview/simulatedairspace.h"

#include <QDebug>

namespace {
// There is one practice aircraft. The list is here because a radio can
// carry several, and the ground station has to be able to tell them
// apart.
constexpr int kPracticeVehicleIdentifier = 1;
}

SimulatedAirspace::SimulatedAirspace(QObject *parent)
    : QObject(parent)
    , m_stepTimer(this)
{
    m_vehicles.push_back(std::make_unique<SimulatedVehicle>(kPracticeVehicleIdentifier));

    m_stepTimer.setInterval(kStepMilliseconds);
    connect(&m_stepTimer, &QTimer::timeout, this, &SimulatedAirspace::stepEveryVehicle);
}

SimulatedAirspace::~SimulatedAirspace() = default;

void SimulatedAirspace::startTime()
{
    m_stepTimer.start();
}

void SimulatedAirspace::stopTime()
{
    m_stepTimer.stop();
}

void SimulatedAirspace::stepEveryVehicle()
{
    const double stepSeconds = kStepMilliseconds / 1000.0;
    for (const auto &vehicle : m_vehicles)
        vehicle->advanceOneStep(stepSeconds);
}

SimulatedVehicle *SimulatedAirspace::vehicleWithIdentifier(int vehicleIdentifier) const
{
    for (const auto &vehicle : m_vehicles) {
        if (vehicle->vehicleIdentifier() == vehicleIdentifier)
            return vehicle.get();
    }
    return nullptr;
}

QList<VehicleSitRep> SimulatedAirspace::currentSitReps() const
{
    QList<VehicleSitRep> sitReps;
    sitReps.reserve(static_cast<int>(m_vehicles.size()));
    for (const auto &vehicle : m_vehicles)
        sitReps.append(vehicle->currentSitRep());
    return sitReps;
}

VehicleCommandAcknowledgment SimulatedAirspace::deliverCommand(const VehicleCommandRequest &request)
{
    SimulatedVehicle *vehicle = vehicleWithIdentifier(request.vehicleIdentifier);

    if (!vehicle) {
        qWarning() << "SimulatedAirspace::deliverCommand: no vehicle numbered"
                   << request.vehicleIdentifier << "is out here, so"
                   << request.commandName << "went nowhere";

        VehicleCommandAcknowledgment acknowledgment;
        acknowledgment.vehicleIdentifier = request.vehicleIdentifier;
        acknowledgment.requestIdentifier = request.requestIdentifier;
        acknowledgment.commandName = request.commandName;
        acknowledgment.wasAccepted = false;
        acknowledgment.refusalReason = QStringLiteral("no such vehicle");
        return acknowledgment;
    }

    return vehicle->answerCommand(request);
}
