#include "modelview/mocklink.h"

#include <QDebug>

namespace {
// 50 times a second. Twenty milliseconds between steps.
constexpr int kAircraftStepMilliseconds = 20;

// 20 times a second. Fifty milliseconds between sends.
constexpr int kSnapshotPublishMilliseconds = 50;

constexpr int kLowBatteryWarningPercent = 25;
}

MockLink::MockLink(QObject *parent)
    : LinkInterface(parent)
    , m_aircraftStepTimer(this)
    , m_snapshotPublishTimer(this)
{
    m_aircraftStepTimer.setInterval(kAircraftStepMilliseconds);
    m_snapshotPublishTimer.setInterval(kSnapshotPublishMilliseconds);

    connect(&m_aircraftStepTimer, &QTimer::timeout,
            this, &MockLink::advanceSimulatedAircraft);
    connect(&m_snapshotPublishTimer, &QTimer::timeout,
            this, &MockLink::publishLatestSnapshot);
}

QString MockLink::linkName() const
{
    return QStringLiteral("Mock Link (no real radio)");
}

void MockLink::startLink()
{
    m_aircraftStepTimer.start();
    m_snapshotPublishTimer.start();
    reportVehicleMessage(AlertSeverity::Information, QStringLiteral("Link up"));
}

void MockLink::stopLink()
{
    m_aircraftStepTimer.stop();
    m_snapshotPublishTimer.stop();
    reportVehicleMessage(AlertSeverity::Critical, QStringLiteral("Link dropped"));
}

void MockLink::sendCommand(const QString &commandName)
{
    const bool wasAccepted = m_vtolFlightSimulator.tryToApplyCommand(commandName);

    if (!wasAccepted) {
        qWarning() << "MockLink::sendCommand: the vehicle refused"
                   << commandName << "- it does not make sense in flight mode"
                   << m_vtolFlightSimulator.currentTelemetrySnapshot().flightModeName;
    }

    // The refusal is NOT announced here. This object reports what the
    // link and the vehicle do on their own. What happened to an order
    // the operator gave is the command panel's story to tell, and it
    // tells it when this answer arrives. Saying it in both places put
    // the same refusal in the message list twice.
    emit commandAcknowledged(commandName, wasAccepted);
}

void MockLink::advanceSimulatedAircraft()
{
    // Runs 50 times a second. Only moves the pretend aircraft. Never
    // sends anything. Sending is the other timer's job.
    m_vtolFlightSimulator.advanceOneStep(kAircraftStepMilliseconds / 1000.0);
}

void MockLink::publishLatestSnapshot()
{
    const TelemetrySnapshot snapshot = m_vtolFlightSimulator.currentTelemetrySnapshot();

    if (snapshot.flightModeName != m_lastReportedFlightModeName) {
        m_lastReportedFlightModeName = snapshot.flightModeName;
        reportVehicleMessage(AlertSeverity::Information,
                      QStringLiteral("Flight mode is now %1").arg(snapshot.flightModeName));
    }

    if (!m_hasWarnedAboutLowBattery && snapshot.batteryPercent <= kLowBatteryWarningPercent) {
        m_hasWarnedAboutLowBattery = true;
        reportVehicleMessage(AlertSeverity::Warning,
                      QStringLiteral("Battery down to %1 percent").arg(snapshot.batteryPercent));
    }

    emit telemetrySnapshotReady(snapshot);
}

void MockLink::reportVehicleMessage(AlertSeverity severity, const QString &messageText)
{
    emit linkMessageReceived(static_cast<int>(severity), messageText);
}
