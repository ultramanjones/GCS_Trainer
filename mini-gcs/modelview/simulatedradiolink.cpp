#include "modelview/simulatedradiolink.h"

#include <QDateTime>
#include <QDebug>

#include "modelview/simulatedairspace.h"

SimulatedRadioLink::SimulatedRadioLink(SimulatedAirspace *simulatedAirspace, QObject *parent)
    : RadioLinkInterface(parent)
    , m_simulatedAirspace(simulatedAirspace)
    , m_listenTimer(this)
    , m_watchdogTimer(this)
{
    m_listenTimer.setInterval(kListenMilliseconds);
    m_watchdogTimer.setInterval(kWatchdogMilliseconds);

    connect(&m_listenTimer, &QTimer::timeout, this, &SimulatedRadioLink::listenForTraffic);
    connect(&m_watchdogTimer, &QTimer::timeout,
            this, &SimulatedRadioLink::checkForVehiclesGoneQuiet);
}

QString SimulatedRadioLink::radioLinkName() const
{
    return QStringLiteral("Simulated radio link");
}

void SimulatedRadioLink::startListening()
{
    m_listenTimer.start();

    // The watchdog runs whether or not we are listening. That is the
    // whole point of it: it has to keep running to notice that nothing
    // is coming in.
    m_watchdogTimer.start();
}

void SimulatedRadioLink::stopListening()
{
    // Unplug the radio. The watchdog is deliberately left running, so
    // a moment later it notices the silence and says so.
    m_listenTimer.stop();
}

void SimulatedRadioLink::listenForTraffic()
{
    if (!m_simulatedAirspace) {
        qWarning() << "SimulatedRadioLink::listenForTraffic: this link has no airspace to listen to";
        return;
    }

    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();

    const QList<VehicleSitRep> sitReps = m_simulatedAirspace->currentSitReps();
    for (const VehicleSitRep &sitRep : sitReps) {
        const int vehicleIdentifier = sitRep.vehicleIdentifier;

        m_lastHeardFromMilliseconds[vehicleIdentifier] = nowMilliseconds;

        if (m_hasBeenReportedLost.value(vehicleIdentifier, false)) {
            m_hasBeenReportedLost[vehicleIdentifier] = false;
            emit contactRegainedWithVehicle(vehicleIdentifier);
        }

        emit vehicleSitRepReceived(sitRep);
    }
}

void SimulatedRadioLink::checkForVehiclesGoneQuiet()
{
    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();

    for (auto iterator = m_lastHeardFromMilliseconds.constBegin();
         iterator != m_lastHeardFromMilliseconds.constEnd(); ++iterator) {

        const int vehicleIdentifier = iterator.key();
        const qint64 quietForMilliseconds = nowMilliseconds - iterator.value();

        if (quietForMilliseconds < kQuietForTooLongMilliseconds)
            continue;

        if (m_hasBeenReportedLost.value(vehicleIdentifier, false))
            continue;  // already said so once

        m_hasBeenReportedLost[vehicleIdentifier] = true;

        qWarning() << "SimulatedRadioLink::checkForVehiclesGoneQuiet: nothing heard from vehicle"
                   << vehicleIdentifier << "for" << quietForMilliseconds << "milliseconds";

        emit contactLostWithVehicle(vehicleIdentifier);
    }
}

void SimulatedRadioLink::sendVehicleCommand(VehicleCommandRequest request)
{
    if (!m_simulatedAirspace) {
        qWarning() << "SimulatedRadioLink::sendVehicleCommand: this link has no airspace, so"
                   << request.commandName << "went nowhere";
        return;
    }

    // A radio with nothing on the other end cannot deliver an order.
    // The ground station's own clock is what notices that, which is
    // why nothing is faked up here.
    if (!m_listenTimer.isActive()) {
        qWarning() << "SimulatedRadioLink::sendVehicleCommand: the radio is unplugged, so"
                   << request.commandName << "was not sent";
        return;
    }

    emit vehicleCommandAcknowledged(m_simulatedAirspace->deliverCommand(request));
}
