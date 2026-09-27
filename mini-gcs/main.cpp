#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QThread>

#include "model/vehiclecommand.h"
#include "model/vehiclesitrep.h"
#include "modelview/groundcontrolstation.h"
#include "modelview/radiolinkinterface.h"
#include "modelview/simulatedairspace.h"
#include "modelview/simulatedradiolink.h"
#include "viewmodel/alertlistviewmodel.h"
#include "viewmodel/vehiclecommandviewmodel.h"
#include "viewmodel/vehiclegroundtrackviewmodel.h"
#include "viewmodel/vehiclestatusviewmodel.h"

// The real radio is only compiled in when the project is configured
// with GCS_USE_MAVLINK_RADIO turned on. See mini-gcs/CMakeLists.txt.
#ifdef GCS_USE_MAVLINK_RADIO
#include "mavlink/mavlinkradiolink.h"
#endif

// THE COMPOSITION ROOT.
//
// The one and only place in this program where objects are built and
// wired to each other. It builds from the deepest layer up: the
// airspace and the radio first, then the ground station, then the view
// models, then the window. No other file creates any of these objects
// and no other file connects any of them. Read this file and you know
// the whole system.
//
// There is no dependency injection framework here on purpose. A person
// wires the constructors together. That is the whole system.
int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    // Pick the plain Qt control style on purpose.
    //
    // On Windows, Qt defaults to a style that draws controls the way
    // the operating system does, and that style refuses to let a
    // program change how a control looks. Our colors get thrown away
    // and a label can come out white on a white button. The Basic
    // style honors what we ask for and looks the same on every
    // machine. This has to be set before any QML loads.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Types that cross a thread through a queued connection have to be
    // registered first, or Qt cannot copy them into the queue.
    qRegisterMetaType<VehicleSitRep>();
    qRegisterMetaType<VehicleCommandRequest>();
    qRegisterMetaType<VehicleCommandAcknowledgment>();

    // ---- The far end of the radio, on its own thread ----
    //
    // Both of these are made on the heap with no parent on purpose. A
    // Qt parent has to live on the same thread as its child, and these
    // are about to move to a different thread than everything else in
    // this function. They delete themselves when the thread finishes,
    // through the connection made further down.
    QThread radioThread;

#ifdef GCS_USE_MAVLINK_RADIO
    // The real radio. It listens for MAVLink frames on a UDP port and
    // there is no simulated airspace at all, because the vehicle is a
    // separate program on the other end of the socket.
    SimulatedAirspace *simulatedAirspace = nullptr;
  #ifdef GCS_MAVLINK_TCP
    // Dial out. Mission Planner's built in simulator offers this.
    // Port 5760 is taken by Mission Planner itself. 5762 and 5763 are
    // left open for other ground stations.
    RadioLinkInterface *radioLink = new MavlinkRadioLink(QStringLiteral("127.0.0.1"), 5762);
  #else
    // Bind a port and wait. This is what a radio, a wifi telemetry
    // bridge, or our own vehicle simulator sends to.
    RadioLinkInterface *radioLink = new MavlinkRadioLink(quint16(14550));
  #endif
#else
    // The fake radio. The vehicle lives in memory in this same
    // program and nothing touches the network.
    auto *simulatedAirspace = new SimulatedAirspace();
    RadioLinkInterface *radioLink = new SimulatedRadioLink(simulatedAirspace);
#endif

    // Whichever one it is, everything below this point is identical.
    // The rest of the program only ever sees RadioLinkInterface.
    if (simulatedAirspace != nullptr) {
        simulatedAirspace->moveToThread(&radioThread);
    }
    radioLink->moveToThread(&radioThread);

    // ---- The ground ----
    GroundControlStation groundControlStation(radioLink);

    VehicleStatusViewModel vehicleStatusViewModel(&groundControlStation);
    VehicleGroundTrackViewModel vehicleGroundTrackViewModel(&groundControlStation);
    VehicleCommandViewModel vehicleCommandViewModel(&groundControlStation);
    AlertListViewModel alertListViewModel;

    // ---- Wiring, part one: traffic coming in from the radio ----
    //
    // Qt::QueuedConnection is written out by hand instead of being
    // left to Qt::AutoConnection. The radio and the ground station sit
    // on different threads for the whole life of this program. There
    // is nothing for Qt to guess, and writing it out tells the next
    // reader what is going on.
    QObject::connect(radioLink, &RadioLinkInterface::vehicleSitRepReceived,
                     &groundControlStation, &GroundControlStation::receiveSitRep,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &RadioLinkInterface::vehicleCommandAcknowledged,
                     &groundControlStation, &GroundControlStation::receiveCommandAcknowledgment,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &RadioLinkInterface::contactLostWithVehicle,
                     &groundControlStation, &GroundControlStation::noteContactLostWithVehicle,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &RadioLinkInterface::contactRegainedWithVehicle,
                     &groundControlStation, &GroundControlStation::noteContactRegainedWithVehicle,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &RadioLinkInterface::vehicleLinkPathChanged,
                     &groundControlStation, &GroundControlStation::noteVehicleLinkPathChanged,
                     Qt::QueuedConnection);

    // ---- Wiring, part two: orders going out to the radio ----
    QObject::connect(&groundControlStation, &GroundControlStation::vehicleCommandReadyToSend,
                     radioLink, &RadioLinkInterface::sendVehicleCommand,
                     Qt::QueuedConnection);

    QObject::connect(&groundControlStation, &GroundControlStation::radioUnplugRequested,
                     radioLink, &RadioLinkInterface::stopListening,
                     Qt::QueuedConnection);

    QObject::connect(&groundControlStation, &GroundControlStation::radioReconnectRequested,
                     radioLink, &RadioLinkInterface::startListening,
                     Qt::QueuedConnection);

    // ---- Wiring, part three: the ground station to the screens ----
    //
    // All on the main thread, so these are ordinary direct calls.
    QObject::connect(&groundControlStation, &GroundControlStation::activeMapVehicleChanged,
                     &vehicleStatusViewModel, &VehicleStatusViewModel::attachToActiveVehicle);
    QObject::connect(&groundControlStation, &GroundControlStation::activeMapVehicleChanged,
                     &vehicleGroundTrackViewModel, &VehicleGroundTrackViewModel::attachToActiveVehicle);

    QObject::connect(&groundControlStation, &GroundControlStation::activeVehicleSitRepApplied,
                     &vehicleStatusViewModel, &VehicleStatusViewModel::refreshFromMapVehicle);
    QObject::connect(&groundControlStation, &GroundControlStation::activeVehicleSitRepApplied,
                     &vehicleGroundTrackViewModel, &VehicleGroundTrackViewModel::refreshFromMapVehicle);

    QObject::connect(&groundControlStation, &GroundControlStation::activeVehicleContactStateChanged,
                     &vehicleStatusViewModel, &VehicleStatusViewModel::refreshFromMapVehicle);
    QObject::connect(&groundControlStation, &GroundControlStation::activeVehicleContactStateChanged,
                     &vehicleGroundTrackViewModel, &VehicleGroundTrackViewModel::refreshFromMapVehicle);

    QObject::connect(&groundControlStation, &GroundControlStation::commandOutcomeKnown,
                     &vehicleCommandViewModel, &VehicleCommandViewModel::showCommandOutcome);

    QObject::connect(&groundControlStation, &GroundControlStation::operatorMessageRaised,
                     &alertListViewModel, &AlertListViewModel::appendAlert);

    // ---- Start the far end ----
    //
    // Time and listening both start only once the worker thread is
    // really running its own event loop. Starting them one line
    // earlier would run their timers on the main thread by mistake.
    if (simulatedAirspace != nullptr) {
        QObject::connect(&radioThread, &QThread::started,
                         simulatedAirspace, &SimulatedAirspace::startTime);
    }
    QObject::connect(&radioThread, &QThread::started,
                     radioLink, &RadioLinkInterface::startListening);

    // Both objects delete themselves on their OWN thread. A QObject's
    // timers belong to the thread the object lives on, and deleting it
    // from another thread makes Qt complain on the way out.
    QObject::connect(&radioThread, &QThread::finished,
                     radioLink, &QObject::deleteLater);
    if (simulatedAirspace != nullptr) {
        QObject::connect(&radioThread, &QThread::finished,
                         simulatedAirspace, &QObject::deleteLater);
    }

    radioThread.start();

    // ---- The window ----
    QQmlApplicationEngine qmlEngine;

    qmlEngine.rootContext()->setContextProperty(QStringLiteral("vehicleStatusViewModel"),
                                                &vehicleStatusViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("vehicleGroundTrackViewModel"),
                                                &vehicleGroundTrackViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("vehicleCommandViewModel"),
                                                &vehicleCommandViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("alertListViewModel"),
                                                &alertListViewModel);

    QObject::connect(&qmlEngine, &QQmlApplicationEngine::objectCreationFailed,
                     &application, []() { QCoreApplication::exit(-1); },
                     Qt::QueuedConnection);

    qmlEngine.loadFromModule("MiniGcs", "Main");

    if (qmlEngine.rootObjects().isEmpty()) {
        qWarning("main: the QML window failed to load, so there is nothing to show");
        return -1;
    }

    const int exitCode = application.exec();

    // Shut the worker thread down. quit asks its event loop to stop.
    // wait blocks until it really has. The two objects out there
    // delete themselves on the way, through the connections above.
    radioThread.quit();
    radioThread.wait();

    return exitCode;
}
