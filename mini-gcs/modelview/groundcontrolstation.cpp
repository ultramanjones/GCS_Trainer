#include "modelview/groundcontrolstation.h"

#include <QDebug>

#include "modelview/mapvehicle.h"
#include "modelview/radiolinkinterface.h"

GroundControlStation::GroundControlStation(RadioLinkInterface *radioLink, QObject *parent)
    : QObject(parent)
    , m_radioLink(radioLink)
    , m_answerTimeoutTimer(this)
{
    m_answerTimeoutTimer.setSingleShot(true);
    m_answerTimeoutTimer.setInterval(kAnswerTimeoutMilliseconds);
    connect(&m_answerTimeoutTimer, &QTimer::timeout,
            this, &GroundControlStation::giveUpOnPendingCommand);
}

QString GroundControlStation::radioLinkName() const
{
    return m_radioLink ? m_radioLink->radioLinkName() : QStringLiteral("no radio");
}

MapVehicle *GroundControlStation::activeMapVehicle() const
{
    return m_activeMapVehicle;
}

MapVehicle *GroundControlStation::mapVehicleWithIdentifier(int vehicleIdentifier)
{
    for (MapVehicle *mapVehicle : m_mapVehicles) {
        if (mapVehicle->vehicleIdentifier() == vehicleIdentifier)
            return mapVehicle;
    }

    // First time we have heard from this one. The ground station learns
    // that a vehicle exists by being told about it, which is how it
    // works in reality.
    auto *newMapVehicle = new MapVehicle(vehicleIdentifier, this);
    m_mapVehicles.append(newMapVehicle);

    if (!m_activeMapVehicle) {
        m_activeMapVehicle = newMapVehicle;
        emit activeMapVehicleChanged();
    }

    raiseMessage(AlertSeverity::Information,
                 QStringLiteral("Vehicle %1 is on the air").arg(vehicleIdentifier));

    return newMapVehicle;
}

void GroundControlStation::raiseMessage(AlertSeverity severity, const QString &messageText)
{
    emit operatorMessageRaised(static_cast<int>(severity), messageText);
}

void GroundControlStation::receiveSitRep(VehicleSitRep sitRep)
{
    MapVehicle *mapVehicle = mapVehicleWithIdentifier(sitRep.vehicleIdentifier);
    mapVehicle->applySitRep(sitRep);
    watchForThingsWorthSaying();

    if (mapVehicle == m_activeMapVehicle)
        emit activeVehicleSitRepApplied();
}

void GroundControlStation::watchForThingsWorthSaying()
{
    if (!m_activeMapVehicle)
        return;

    const QString flightModeName = m_activeMapVehicle->flightModeName();
    if (flightModeName != m_lastAnnouncedFlightModeName) {
        m_lastAnnouncedFlightModeName = flightModeName;

        // Most mode changes are routine. Losing the motors and hitting
        // the ground are not, and they are logged where the operator
        // cannot scroll past without noticing.
        const bool isSerious = flightModeName == QLatin1String("MOTORS CUT")
                            || flightModeName == QLatin1String("Crashed");

        raiseMessage(isSerious ? AlertSeverity::Critical : AlertSeverity::Information,
                     QStringLiteral("Flight mode is now %1").arg(flightModeName));
    }

    const int chargePercent =
        static_cast<int>(qRound(m_activeMapVehicle->lastConfirmedBattery().chargePercent()));

    if (!m_hasWarnedAboutLowBattery && chargePercent <= kLowBatteryWarningPercent) {
        m_hasWarnedAboutLowBattery = true;
        raiseMessage(AlertSeverity::Warning,
                     QStringLiteral("Battery down to %1 percent").arg(chargePercent));
    } else if (m_hasWarnedAboutLowBattery && chargePercent > kLowBatteryWarningPercent) {
        m_hasWarnedAboutLowBattery = false;  // a fresh battery re-arms the warning
    }
}

void GroundControlStation::noteContactLostWithVehicle(int vehicleIdentifier)
{
    MapVehicle *mapVehicle = mapVehicleWithIdentifier(vehicleIdentifier);
    mapVehicle->markOutOfContact();

    if (mapVehicle == m_activeMapVehicle)
        emit activeVehicleContactStateChanged();

    raiseMessage(AlertSeverity::Critical,
                 QStringLiteral("Out of contact with vehicle %1").arg(vehicleIdentifier));
}

void GroundControlStation::noteContactRegainedWithVehicle(int vehicleIdentifier)
{
    MapVehicle *mapVehicle = mapVehicleWithIdentifier(vehicleIdentifier);
    mapVehicle->markBackInContact();

    if (mapVehicle == m_activeMapVehicle)
        emit activeVehicleContactStateChanged();

    raiseMessage(AlertSeverity::Information,
                 QStringLiteral("Back in contact with vehicle %1").arg(vehicleIdentifier));
}

void GroundControlStation::noteVehicleLinkPathChanged(int vehicleIdentifier,
                                                      int severityValue,
                                                      QString noticeText)
{
    // The radio decides what happened to its paths. The ground station
    // only passes the news on to the operator.
    raiseMessage(static_cast<AlertSeverity>(severityValue),
                 QStringLiteral("Vehicle %1: %2").arg(QString::number(vehicleIdentifier), noticeText));
}

void GroundControlStation::requestVehicleCommand(QString commandName)
{
    if (!m_activeMapVehicle) {
        qWarning() << "GroundControlStation::requestVehicleCommand: no vehicle is being tracked, so"
                   << commandName << "has nowhere to go";
        raiseMessage(AlertSeverity::Warning, QStringLiteral("No vehicle to send %1 to").arg(commandName));
        return;
    }

    const bool isEmergencyStop = commandName == VehicleCommandName::EmergencyStop;

    if (m_isWaitingForAnswer && !isEmergencyStop) {
        qWarning() << "GroundControlStation::requestVehicleCommand: refused to send"
                   << commandName << "because" << m_pendingRequest.commandName
                   << "has not been answered yet";
        raiseMessage(AlertSeverity::Warning,
                     QStringLiteral("Still waiting on %1").arg(m_pendingRequest.commandName));
        return;
    }

    if (isEmergencyStop && m_isWaitingForAnswer) {
        // Throw away whatever we were waiting on. There is no answer
        // worth waiting for once the operator has decided to stop the
        // motors.
        m_answerTimeoutTimer.stop();
        m_isWaitingForAnswer = false;
    }

    VehicleCommandRequest request;
    request.vehicleIdentifier = m_activeMapVehicle->vehicleIdentifier();
    request.requestIdentifier = m_nextRequestIdentifier++;
    request.commandName = commandName;

    m_pendingRequest = request;
    m_isWaitingForAnswer = true;
    m_answerTimeoutTimer.start();

    if (isEmergencyStop) {
        qWarning() << "GroundControlStation::requestVehicleCommand: the operator cut the motors on vehicle"
                   << request.vehicleIdentifier;
        raiseMessage(AlertSeverity::Critical, QStringLiteral("EMERGENCY STOP - motors cut"));
    }

    emit vehicleCommandReadyToSend(request);
}

void GroundControlStation::receiveCommandAcknowledgment(VehicleCommandAcknowledgment acknowledgment)
{
    if (!m_isWaitingForAnswer || acknowledgment.requestIdentifier != m_pendingRequest.requestIdentifier) {
        // An answer for an order we are not waiting on. Usually that
        // means it arrived after its clock ran out, and by then the
        // operator has already been told it failed. Log it, drop it.
        qWarning() << "GroundControlStation::receiveCommandAcknowledgment: got answer number"
                   << acknowledgment.requestIdentifier << "for" << acknowledgment.commandName
                   << "but that is not the order being waited on";
        return;
    }

    m_answerTimeoutTimer.stop();
    m_isWaitingForAnswer = false;

    emit commandOutcomeKnown(acknowledgment.commandName,
                             acknowledgment.wasAccepted,
                             acknowledgment.refusalReason);

    if (acknowledgment.wasAccepted) {
        raiseMessage(AlertSeverity::Information,
                     QStringLiteral("%1 accepted").arg(acknowledgment.commandName));
    } else {
        raiseMessage(AlertSeverity::Warning,
                     QStringLiteral("%1 refused: %2")
                         .arg(acknowledgment.commandName, acknowledgment.refusalReason));
    }
}

void GroundControlStation::giveUpOnPendingCommand()
{
    const QString commandThatWentUnanswered = m_pendingRequest.commandName;
    m_isWaitingForAnswer = false;

    qWarning() << "GroundControlStation::giveUpOnPendingCommand: no answer for"
               << commandThatWentUnanswered << "within"
               << kAnswerTimeoutMilliseconds << "milliseconds";

    emit commandOutcomeKnown(commandThatWentUnanswered, false,
                             QStringLiteral("no answer from the vehicle"));

    raiseMessage(AlertSeverity::Critical,
                 QStringLiteral("No answer to %1").arg(commandThatWentUnanswered));
}

void GroundControlStation::requestRadioUnplug()
{
    raiseMessage(AlertSeverity::Warning, QStringLiteral("Radio unplugged on purpose"));
    emit radioUnplugRequested();
}

void GroundControlStation::requestRadioReconnect()
{
    raiseMessage(AlertSeverity::Information, QStringLiteral("Radio plugged back in"));
    emit radioReconnectRequested();
}
