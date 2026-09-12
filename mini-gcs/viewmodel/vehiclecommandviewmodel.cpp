#include "viewmodel/vehiclecommandviewmodel.h"

#include "model/vehiclecommand.h"
#include "modelview/groundcontrolstation.h"

VehicleCommandViewModel::VehicleCommandViewModel(GroundControlStation *groundControlStation,
                                                 QObject *parent)
    : QObject(parent)
    , m_groundControlStation(groundControlStation)
{
}

QString VehicleCommandViewModel::lastCommandResultText() const
{
    return m_lastCommandResultText;
}

void VehicleCommandViewModel::requestArm()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::Arm);
    setLastCommandResultText(QStringLiteral("Arm sent, waiting for the vehicle"));
}

void VehicleCommandViewModel::requestDisarm()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::Disarm);
    setLastCommandResultText(QStringLiteral("Disarm sent, waiting for the vehicle"));
}

void VehicleCommandViewModel::requestLaunch()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::Launch);
    setLastCommandResultText(QStringLiteral("Launch sent, waiting for the vehicle"));
}

void VehicleCommandViewModel::requestReturnToHome()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::ReturnToHome);
    setLastCommandResultText(QStringLiteral("Return sent, waiting for the vehicle"));
}

void VehicleCommandViewModel::requestLand()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::Land);
    setLastCommandResultText(QStringLiteral("Land sent, waiting for the vehicle"));
}

void VehicleCommandViewModel::requestEmergencyStop()
{
    m_groundControlStation->requestVehicleCommand(VehicleCommandName::EmergencyStop);
    setLastCommandResultText(QStringLiteral("Motors cut by the operator"));
}

void VehicleCommandViewModel::requestRadioUnplug()
{
    m_groundControlStation->requestRadioUnplug();
    setLastCommandResultText(QStringLiteral("Radio unplugged on purpose"));
}

void VehicleCommandViewModel::requestRadioReconnect()
{
    m_groundControlStation->requestRadioReconnect();
    setLastCommandResultText(QStringLiteral("Radio plugged back in"));
}

void VehicleCommandViewModel::showCommandOutcome(QString commandName, bool wasAccepted, QString reason)
{
    if (wasAccepted) {
        setLastCommandResultText(QStringLiteral("%1 accepted").arg(commandName));
        return;
    }

    // A refusal says what happened and why. "Refused" on its own leaves
    // the operator guessing.
    if (reason.isEmpty())
        setLastCommandResultText(QStringLiteral("%1 refused").arg(commandName));
    else
        setLastCommandResultText(QStringLiteral("%1 refused: %2").arg(commandName, reason));
}

void VehicleCommandViewModel::setLastCommandResultText(const QString &resultText)
{
    if (m_lastCommandResultText == resultText)
        return;
    m_lastCommandResultText = resultText;
    emit lastCommandResultTextChanged();
}
