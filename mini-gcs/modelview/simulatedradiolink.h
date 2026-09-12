#pragma once

#include <QHash>
#include <QTimer>

#include "modelview/radiolinkinterface.h"

class SimulatedAirspace;

// A radio whose far end is a simulation.
//
// It holds one reference to the airspace and knows nothing else about
// aircraft. A radio carries messages. It does not own the thing on the
// other end of the wire, in this program or in reality.
//
// Two timers run here. One listens, twenty times a second. The other
// is a watchdog that notices when a vehicle has stopped talking.
//
// Listening twenty times a second instead of fifty is coalescing. The
// vehicles step fifty times a second, but the screen does not need
// fifty updates, and every extra update costs real work on the thread
// that draws the window.
//
// QGroundControl calls its version of this MockLink.
class SimulatedRadioLink : public RadioLinkInterface
{
    Q_OBJECT

public:
    explicit SimulatedRadioLink(SimulatedAirspace *simulatedAirspace, QObject *parent = nullptr);

    QString radioLinkName() const override;

public slots:
    void startListening() override;
    void stopListening() override;
    void sendVehicleCommand(VehicleCommandRequest request) override;

private slots:
    void listenForTraffic();
    void checkForVehiclesGoneQuiet();

private:
    // Not owned. The composition root owns the airspace.
    SimulatedAirspace *m_simulatedAirspace = nullptr;

    QTimer m_listenTimer;
    QTimer m_watchdogTimer;

    // When each vehicle was last heard from, and whether it has
    // already been reported as lost so it is only reported once.
    QHash<int, qint64> m_lastHeardFromMilliseconds;
    QHash<int, bool> m_hasBeenReportedLost;

    static constexpr int kListenMilliseconds = 50;
    static constexpr int kWatchdogMilliseconds = 250;

    // Quiet for this long and the vehicle is called out of contact.
    static constexpr qint64 kQuietForTooLongMilliseconds = 1500;
};
