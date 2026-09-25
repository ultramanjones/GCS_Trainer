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
    QThread simulatedAirspaceThread;
    auto *simulatedAirspace = new SimulatedAirspace();
    auto *simulatedRadioLink = new SimulatedRadioLink(simulatedAirspace);

    simulatedAirspace->moveToThread(&simulatedAirspaceThread);
    simulatedRadioLink->moveToThread(&simulatedAirspaceThread);

    // ---- The ground ----
    GroundControlStation groundControlStation(simulatedRadioLink);

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
    QObject::connect(simulatedRadioLink, &RadioLinkInterface::vehicleSitRepReceived,
                     &groundControlStation, &GroundControlStation::receiveSitRep,
                     Qt::QueuedConnection);

    QObject::connect(simulatedRadioLink, &RadioLinkInterface::vehicleCommandAcknowledged,
                     &groundControlStation, &GroundControlStation::receiveCommandAcknowledgment,
                     Qt::QueuedConnection);

    QObject::connect(simulatedRadioLink, &RadioLinkInterface::contactLostWithVehicle,
                     &groundControlStation, &GroundControlStation::noteContactLostWithVehicle,
                     Qt::QueuedConnection);

    QObject::connect(simulatedRadioLink, &RadioLinkInterface::contactRegainedWithVehicle,
                     &groundControlStation, &GroundControlStation::noteContactRegainedWithVehicle,
                     Qt::QueuedConnection);

    // ---- Wiring, part two: orders going out to the radio ----
    QObject::connect(&groundControlStation, &GroundControlStation::vehicleCommandReadyToSend,
                     simulatedRadioLink, &RadioLinkInterface::sendVehicleCommand,
                     Qt::QueuedConnection);

    QObject::connect(&groundControlStation, &GroundControlStation::radioUnplugRequested,
                     simulatedRadioLink, &RadioLinkInterface::stopListening,
                     Qt::QueuedConnection);

    QObject::connect(&groundControlStation, &GroundControlStation::radioReconnectRequested,
                     simulatedRadioLink, &RadioLinkInterface::startListening,
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
    QObject::connect(&simulatedAirspaceThread, &QThread::started,
                     simulatedAirspace, &SimulatedAirspace::startTime);
    QObject::connect(&simulatedAirspaceThread, &QThread::started,
                     simulatedRadioLink, &RadioLinkInterface::startListening);

    // Both objects delete themselves on their OWN thread. A QObject's
    // timers belong to the thread the object lives on, and deleting it
    // from another thread makes Qt complain on the way out.
    QObject::connect(&simulatedAirspaceThread, &QThread::finished,
                     simulatedRadioLink, &QObject::deleteLater);
    QObject::connect(&simulatedAirspaceThread, &QThread::finished,
                     simulatedAirspace, &QObject::deleteLater);

    simulatedAirspaceThread.start();

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
    simulatedAirspaceThread.quit();
    simulatedAirspaceThread.wait();

    return exitCode;
}
