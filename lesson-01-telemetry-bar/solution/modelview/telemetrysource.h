#pragma once

#include <QObject>
#include <QTimer>

#include "model/telemetrysnapshot.h"

// TelemetrySource stands in for a real vehicle data link (MAVLink, in a
// real GCS). It lives on its own worker thread so a slow network or a
// slow parse can never freeze the screen.
//
// It runs two timers:
//   - m_readingTimer, at 50 Hz, makes one new fake reading. A real
//     link would parse one incoming packet here instead.
//   - m_publishTimer, at 20 Hz, packs the latest reading into one
//     TelemetrySnapshot and emits ONE signal. This is "coalescing":
//     QML never sees more than 20 updates a second, no matter how
//     fast the fake data actually changes underneath.
class TelemetrySource : public QObject
{
    Q_OBJECT

public:
    explicit TelemetrySource(QObject *parent = nullptr);

public slots:
    // Call this only after moveToThread has run and the worker thread
    // has actually started (main.cpp connects QThread::started to
    // this slot — never call it directly). Starting the timers before
    // the thread starts leaves them running on the wrong thread.
    void start();

    // Stops both timers, simulating a dropped link. Snapshots stop
    // arriving; the receiver has to notice on its own that time is
    // passing with no new data.
    void stopLink();

signals:
    // The only way data leaves this object. Always emitted from the
    // worker thread. Qt::QueuedConnection on the receiving end is
    // what moves the call safely onto the main thread.
    void snapshotReady(TelemetrySnapshot snapshot);

private slots:
    void makeReading();
    void publishSnapshot();

private:
    TelemetrySnapshot m_latestReading;

    // Both timers take this object as their parent in the
    // constructor. moveToThread() moves an object and its children,
    // and nothing else. A timer with no parent would stay behind on
    // the main thread, and start() would then fail at run time with
    // "QObject::startTimer: Timers cannot be started from another
    // thread" — the program still runs, but no data ever arrives.
    QTimer m_readingTimer;
    QTimer m_publishTimer;

    int m_tickCount = 0;
};
