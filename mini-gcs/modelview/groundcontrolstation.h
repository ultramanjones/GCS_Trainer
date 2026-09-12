#pragma once

#include <QList>
#include <QObject>
#include <QString>
#include <QTimer>

#include "model/vehiclealert.h"
#include "model/vehiclecommand.h"
#include "model/vehiclesitrep.h"

class MapVehicle;
class RadioLinkInterface;

// The ground station itself.
//
// It owns the radio and the vehicles it is tracking. It is the thing
// sitting on the table, and it exists as an object even with one
// aircraft, because that is what it is in reality.
//
// This lives in the modelview layer, not the viewmodel layer, because
// it holds state that outlives any screen. Close every panel and the
// ground station is still tracking the aircraft.
//
// QGroundControl splits this job between MultiVehicleManager and its
// application singleton.
class GroundControlStation : public QObject
{
    Q_OBJECT

public:
    explicit GroundControlStation(RadioLinkInterface *radioLink, QObject *parent = nullptr);

    QString radioLinkName() const;

    // The vehicle the screens are showing. With one aircraft this is
    // always the same one.
    MapVehicle *activeMapVehicle() const;

public slots:
    // Traffic coming in from the radio. Wired with queued connections
    // in the composition root, because the radio runs on its own
    // thread.
    void receiveSitRep(VehicleSitRep sitRep);
    void receiveCommandAcknowledgment(VehicleCommandAcknowledgment acknowledgment);
    void noteContactLostWithVehicle(int vehicleIdentifier);
    void noteContactRegainedWithVehicle(int vehicleIdentifier);

    // Orders coming down from the screens.
    void requestVehicleCommand(QString commandName);
    void requestRadioUnplug();
    void requestRadioReconnect();

signals:
    void vehicleCommandReadyToSend(VehicleCommandRequest request);
    void radioUnplugRequested();
    void radioReconnectRequested();

    void activeMapVehicleChanged();

    // Passed along for whichever vehicle is being shown. The view
    // models cannot connect to a map vehicle directly, because no map
    // vehicle exists until the first report arrives. Re-emitting here
    // keeps every connection in the composition root, where the house
    // rules say it belongs.
    void activeVehicleSitRepApplied();
    void activeVehicleContactStateChanged();

    // What happened to the last order. The view model turns this into
    // words on a screen; that formatting is not this object's job.
    void commandOutcomeKnown(QString commandName, bool wasAccepted, QString reason);

    // Anything the operator should read, with how serious it is.
    void operatorMessageRaised(int severityValue, QString messageText);

private slots:
    void giveUpOnPendingCommand();

private:
    MapVehicle *mapVehicleWithIdentifier(int vehicleIdentifier);
    void raiseMessage(AlertSeverity severity, const QString &messageText);
    void watchForThingsWorthSaying();

    // Not owned. The composition root builds the radio and wires it.
    RadioLinkInterface *m_radioLink = nullptr;

    QList<MapVehicle *> m_mapVehicles;
    MapVehicle *m_activeMapVehicle = nullptr;

    // Correlating an order with its answer. Every order gets a number,
    // and an answer only counts if the number matches. Without this an
    // answer that arrives late looks exactly like an answer to
    // whatever was sent next.
    int m_nextRequestIdentifier = 1;
    VehicleCommandRequest m_pendingRequest;
    bool m_isWaitingForAnswer = false;
    QTimer m_answerTimeoutTimer;

    // Remembered so the same thing is not said twice.
    QString m_lastAnnouncedFlightModeName;
    bool m_hasWarnedAboutLowBattery = false;

    static constexpr int kAnswerTimeoutMilliseconds = 2000;
    static constexpr int kLowBatteryWarningPercent = 25;
};
