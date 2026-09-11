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
    // than everything else here. Delete it by hand, after the worker
    // thread has actually stopped, once Step 4 is wired up.
    QThread workerThread;
    auto *telemetrySource = new TelemetrySource();
    telemetrySource->moveToThread(&workerThread);

    // --- Main-thread object QML will bind to ---
    VehicleModel vehicleModel(telemetrySource);

    // TODO (Lesson 1, Step 4): wire the worker to the main thread.
    //
    // 1. Connect telemetrySource's snapshotReady signal to
    //    vehicleModel's applySnapshot slot, with Qt::QueuedConnection
    //    forced explicitly (don't rely on Qt::AutoConnection here —
    //    say out loud why forcing it is the safer habit).
    //
    // 2. Connect workerThread's started() signal to telemetrySource's
    //    start() slot, so the timers only start once the thread is
    //    actually running on its own event loop.
    //
    // 3. Call workerThread.start().
    //
    // Do all three before the QML engine starts up below.

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("vehicleModel", &vehicleModel);

    // Load the window by module name, not by a hard-coded resource
    // path. CMake already put Main.qml in the "GcsTrainerLesson01"
    // QML module, so the engine looks it up the same way QML itself
    // would. A typed path here breaks the moment the module layout
    // changes, and it fails at run time, not at build time.
    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("GcsTrainerLesson01", "Main");

    if (engine.rootObjects().isEmpty())
        return -1;

    const int result = app.exec();

    workerThread.quit();
    workerThread.wait();
    return result;
}
