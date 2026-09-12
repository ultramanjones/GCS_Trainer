#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QThread>

#include "model/telemetrysnapshot.h"
#include "modelview/mocklink.h"
#include "viewmodel/alertlistviewmodel.h"
#include "viewmodel/commandpanelviewmodel.h"
#include "viewmodel/flightmapviewmodel.h"
#include "viewmodel/vehiclestatusviewmodel.h"

// THE COMPOSITION ROOT.
//
// This is the one and only place in the program where objects are
// built and wired to each other. It builds them from the bottom up:
// the radio link first, then the view models, then the window. No
// other file creates any of these objects, and no other file connects
// any of them. A person reads this file and knows the whole system.
//
// There is no dependency injection framework here on purpose. A human
// wires the constructors together. That is the whole system.
int main(int argc, char *argv[])
{
    QGuiApplication application(argc, argv);

    // A custom type crossing threads through a queued connection has
    // to be registered first, or Qt cannot copy it into the queue.
    // Pick the plain Qt control style on purpose.
    //
    // On Windows, Qt defaults to a style that draws buttons the way
    // the operating system does. That style refuses to let a program
    // change how a control looks, so our button colors are thrown
    // away and the label can come out white on a white button. The
    // Basic style is the plain one that honors what we ask for, and
    // it looks the same on every machine, which is what a ground
    // station wants. This has to be set before any QML loads.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    qRegisterMetaType<TelemetrySnapshot>();

    // ---- The back end: a radio link on its own thread ----
    //
    // radioLink is made on the heap with no parent on purpose. A Qt
    // parent has to live on the same thread as its child, and this
    // object is about to move to a different thread than everything
    // else in this function. It is deleted by hand at the bottom,
    // after the thread has really stopped.
    QThread radioLinkThread;
    auto *radioLink = new MockLink();
    radioLink->moveToThread(&radioLinkThread);

    // ---- The view models: everything the screen reads ----
    VehicleStatusViewModel vehicleStatusViewModel;
    FlightMapViewModel flightMapViewModel;
    AlertListViewModel alertListViewModel;
    CommandPanelViewModel commandPanelViewModel;

    // ---- Wiring, part one: data coming up from the radio ----
    //
    // Qt::QueuedConnection is forced by hand instead of being left to
    // Qt::AutoConnection. Sender and receiver sit on different
    // threads for the whole life of this program. There is nothing
    // for Qt to guess, and writing it out says so to the next reader.
    QObject::connect(radioLink, &LinkInterface::telemetrySnapshotReady,
                     &vehicleStatusViewModel, &VehicleStatusViewModel::applyTelemetrySnapshot,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &LinkInterface::telemetrySnapshotReady,
                     &flightMapViewModel, &FlightMapViewModel::applyTelemetrySnapshot,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &LinkInterface::commandAcknowledged,
                     &commandPanelViewModel, &CommandPanelViewModel::applyCommandAcknowledgment,
                     Qt::QueuedConnection);

    QObject::connect(radioLink, &LinkInterface::linkMessageReceived,
                     &alertListViewModel, &AlertListViewModel::appendAlert,
                     Qt::QueuedConnection);

    // ---- Wiring, part two: orders going down to the radio ----
    QObject::connect(&commandPanelViewModel, &CommandPanelViewModel::commandRequested,
                     radioLink, &LinkInterface::sendCommand,
                     Qt::QueuedConnection);

    QObject::connect(&commandPanelViewModel, &CommandPanelViewModel::dropLinkRequested,
                     radioLink, &LinkInterface::stopLink,
                     Qt::QueuedConnection);

    QObject::connect(&commandPanelViewModel, &CommandPanelViewModel::reconnectLinkRequested,
                     radioLink, &LinkInterface::startLink,
                     Qt::QueuedConnection);

    // ---- Wiring, part three: two view models that must not touch ----
    //
    // The command panel needs its failures to show up in the alert
    // list. It is not allowed to hold the alert list, because that
    // would be one view model reaching sideways to another. So it
    // raises a signal, and the wiring happens here, in the one place
    // that is allowed to know about both.
    QObject::connect(&commandPanelViewModel, &CommandPanelViewModel::operatorMessageRaised,
                     &alertListViewModel, &AlertListViewModel::appendAlert);

    // The link object is deleted on its OWN thread, not on this one.
    //
    // A QObject's timers belong to the thread the object lives on.
    // Deleting the object from a different thread makes Qt print
    // "Timers cannot be stopped from another thread" on the way out.
    // Connecting the thread's finished signal to deleteLater lets the
    // object clean itself up in the right place, which is the pattern
    // Qt's own documentation uses.
    QObject::connect(&radioLinkThread, &QThread::finished,
                     radioLink, &QObject::deleteLater);

    // The link only starts once its thread is really running its own
    // event loop. Starting it one line earlier would run its timers
    // on the main thread by mistake.
    QObject::connect(&radioLinkThread, &QThread::started,
                     radioLink, &LinkInterface::startLink);

    radioLinkThread.start();

    // ---- The window ----
    QQmlApplicationEngine qmlEngine;

    qmlEngine.rootContext()->setContextProperty(QStringLiteral("vehicleStatusViewModel"), &vehicleStatusViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("flightMapViewModel"), &flightMapViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("alertListViewModel"), &alertListViewModel);
    qmlEngine.rootContext()->setContextProperty(QStringLiteral("commandPanelViewModel"), &commandPanelViewModel);

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
    // wait blocks until it really has. The link object deletes itself
    // on the way out, through the connection made above.
    radioLinkThread.quit();
    radioLinkThread.wait();

    return exitCode;
}
