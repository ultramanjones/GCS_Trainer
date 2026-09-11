#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QThread>

#include "model/telemetrysnapshot.h"
#include "modelview/telemetrysource.h"
#include "viewmodel/vehiclemodel.h"

// This is the one composition root for Lesson 1. It builds everything
// by hand, in order: the worker thread and the object that lives on
// it, then the main-thread object, then the QML engine. Nothing else
// in this program is allowed to create a TelemetrySource or a
// VehicleModel.
int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);

    // A custom struct crossing threads through a queued connection
    // must be registered first, or Qt cannot copy it into its
    // cross-thread event queue.
    qRegisterMetaType<TelemetrySnapshot>();

    // --- Worker thread and the object that lives on it ---
    // telemetrySource is heap-allocated with no QObject parent on
    // purpose: a QObject parent must live on the same thread as its
    // child, and this object is about to move to a different thread
    // than everything else here. It is deleted by hand below, after
    // the worker thread has actually stopped.
    QThread workerThread;
    auto *telemetrySource = new TelemetrySource();
    telemetrySource->moveToThread(&workerThread);

    // --- Main-thread object QML will bind to ---
    VehicleModel vehicleModel(telemetrySource);

    // Wire the worker to the main thread. Qt::QueuedConnection is
    // forced explicitly rather than left as Qt::AutoConnection: the
    // sender and receiver live on different threads for the whole
    // life of this program, so there is no ambiguity to leave to
    // Qt's runtime guess, and forcing it documents the intent for the
    // next person reading this file.
    QObject::connect(telemetrySource, &TelemetrySource::snapshotReady,
                      &vehicleModel, &VehicleModel::applySnapshot,
                      Qt::QueuedConnection);

    // The timers only start once the worker thread is actually
    // running its own event loop — starting them one line earlier
    // would run them on the main thread by mistake.
    QObject::connect(&workerThread, &QThread::started,
                      telemetrySource, &TelemetrySource::start);

    workerThread.start();

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("vehicleModel", &vehicleModel);

    // Load the window by module name, not by a hard-coded resource
    // path. CMake already put Main.qml in the "GcsTrainerLesson01Solution"
    // QML module, so the engine looks it up the same way QML itself
    // would. A typed path here breaks the moment the module layout
    // changes, and it fails at run time, not at build time.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("GcsTrainerLesson01Solution", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    const int result = app.exec();

    workerThread.quit();
    workerThread.wait();
    delete telemetrySource;
    return result;
}
