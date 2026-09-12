#pragma once

#include <QTimer>

#include "model/vehiclealert.h"
#include "modelview/linkinterface.h"
#include "modelview/vtolflightsimulator.h"

// A fake radio link to a fake vehicle.
//
// It lives on its own worker thread so a slow radio or a slow parse
// can never freeze the window. QGroundControl has a class with this
// same name that does the same job.
//
// Two timers run here:
//   The fast one steps the pretend aircraft forward 50 times a
//   second. That is about the rate a real radio delivers packets.
//   The slow one packs the newest readings into one snapshot and
//   sends ONE signal, 20 times a second.
//
// Sending 20 times instead of 50 is called coalescing. The screen
// does not need 50 updates a second, and every extra update costs
// real work on the thread that draws the window.
class MockLink : public LinkInterface
{
    Q_OBJECT

public:
    explicit MockLink(QObject *parent = nullptr);

    QString linkName() const override;

public slots:
    void startLink() override;
    void stopLink() override;
    void sendCommand(const QString &commandName) override;

private slots:
    void advanceSimulatedAircraft();
    void publishLatestSnapshot();

private:
    void reportVehicleMessage(AlertSeverity severity, const QString &messageText);

    VtolFlightSimulator m_vtolFlightSimulator;

    // Both timers take this object as their parent. moveToThread
    // moves an object and its children and nothing else. A timer with
    // no parent would stay behind on the main thread, and starting it
    // from the worker thread would fail.
    QTimer m_aircraftStepTimer;
    QTimer m_snapshotPublishTimer;

    // Used to notice when the flight mode changed, so a message can
    // be written to the alert list.
    QString m_lastReportedFlightModeName;

    // Used to warn once when the battery crosses a line.
    bool m_hasWarnedAboutLowBattery = false;
};
