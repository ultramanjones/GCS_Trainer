#include "viewmodel/commandpanelviewmodel.h"

#include <QDebug>

#include "model/vehiclealert.h"

CommandPanelViewModel::CommandPanelViewModel(QObject *parent)
    : QObject(parent)
    , m_acknowledgmentTimeoutTimer(this)
{
    m_acknowledgmentTimeoutTimer.setSingleShot(true);
    m_acknowledgmentTimeoutTimer.setInterval(kAcknowledgmentTimeoutMilliseconds);
    connect(&m_acknowledgmentTimeoutTimer, &QTimer::timeout,
            this, &CommandPanelViewModel::handleAcknowledgmentTimeout);
}

QString CommandPanelViewModel::pendingCommandName() const { return m_pendingCommandName; }
bool CommandPanelViewModel::isWaitingForAcknowledgment() const { return !m_pendingCommandName.isEmpty(); }
QString CommandPanelViewModel::lastCommandResultText() const { return m_lastCommandResultText; }

void CommandPanelViewModel::requestArm()            { sendCommandAndStartClock(QStringLiteral("Arm")); }
void CommandPanelViewModel::requestDisarm()         { sendCommandAndStartClock(QStringLiteral("Disarm")); }
void CommandPanelViewModel::requestLaunch()         { sendCommandAndStartClock(QStringLiteral("Launch")); }
void CommandPanelViewModel::requestReturnToLaunch() { sendCommandAndStartClock(QStringLiteral("Return")); }
void CommandPanelViewModel::requestLand()           { sendCommandAndStartClock(QStringLiteral("Land")); }

void CommandPanelViewModel::requestEmergencyStop()
{
    // Throw away whatever we were waiting on. There is no answer worth
    // waiting for once the operator has decided to stop the motors.
    m_acknowledgmentTimeoutTimer.stop();
    setPendingCommandName(QString());

    qWarning() << "CommandPanelViewModel::requestEmergencyStop: the operator cut the motors";

    setLastCommandResultText(QStringLiteral("Motors cut by the operator"));
    emit operatorMessageRaised(static_cast<int>(AlertSeverity::Critical),
                               QStringLiteral("EMERGENCY STOP - motors cut"));

    emit commandRequested(QStringLiteral("EmergencyStop"));
}

void CommandPanelViewModel::requestDropLink()
{
    emit dropLinkRequested();
    setLastCommandResultText(QStringLiteral("Radio unplugged on purpose"));
}

void CommandPanelViewModel::requestReconnectLink()
{
    emit reconnectLinkRequested();
    setLastCommandResultText(QStringLiteral("Radio plugged back in"));
}

void CommandPanelViewModel::sendCommandAndStartClock(const QString &commandName)
{
    if (isWaitingForAcknowledgment()) {
        qWarning() << "CommandPanelViewModel::sendCommandAndStartClock: refused to send"
                   << commandName << "because" << m_pendingCommandName
                   << "has not been answered yet";
        emit operatorMessageRaised(static_cast<int>(AlertSeverity::Warning),
                                   QStringLiteral("Still waiting on %1").arg(m_pendingCommandName));
        return;
    }

    setPendingCommandName(commandName);
    setLastCommandResultText(QStringLiteral("%1 sent, waiting for the vehicle").arg(commandName));

    emit commandRequested(commandName);
    m_acknowledgmentTimeoutTimer.start();
}

void CommandPanelViewModel::applyCommandAcknowledgment(QString commandName, bool wasAccepted)
{
    if (commandName != m_pendingCommandName) {
        // An answer arrived for an order we are not waiting on. That
        // usually means an answer came in after its clock ran out.
        qWarning() << "CommandPanelViewModel::applyCommandAcknowledgment: got an answer for"
                   << commandName << "but the pending order is"
                   << (m_pendingCommandName.isEmpty() ? QStringLiteral("none") : m_pendingCommandName);
        return;
    }

    m_acknowledgmentTimeoutTimer.stop();
    setPendingCommandName(QString());

    if (wasAccepted) {
        setLastCommandResultText(QStringLiteral("%1 accepted").arg(commandName));
        emit operatorMessageRaised(static_cast<int>(AlertSeverity::Information),
                                   QStringLiteral("%1 accepted").arg(commandName));
    } else {
        setLastCommandResultText(QStringLiteral("%1 refused by the vehicle").arg(commandName));
        emit operatorMessageRaised(static_cast<int>(AlertSeverity::Warning),
                                   QStringLiteral("%1 refused by the vehicle").arg(commandName));
    }
}

void CommandPanelViewModel::handleAcknowledgmentTimeout()
{
    const QString commandThatWentUnanswered = m_pendingCommandName;
    setPendingCommandName(QString());

    qWarning() << "CommandPanelViewModel::handleAcknowledgmentTimeout: no answer for"
               << commandThatWentUnanswered << "within"
               << kAcknowledgmentTimeoutMilliseconds << "milliseconds";

    setLastCommandResultText(QStringLiteral("%1 got no answer").arg(commandThatWentUnanswered));
    emit operatorMessageRaised(static_cast<int>(AlertSeverity::Critical),
                               QStringLiteral("No answer to %1").arg(commandThatWentUnanswered));
}

void CommandPanelViewModel::setPendingCommandName(const QString &commandName)
{
    if (m_pendingCommandName == commandName)
        return;
    m_pendingCommandName = commandName;
    emit pendingCommandNameChanged();
}

void CommandPanelViewModel::setLastCommandResultText(const QString &resultText)
{
    if (m_lastCommandResultText == resultText)
        return;
    m_lastCommandResultText = resultText;
    emit lastCommandResultTextChanged();
}
