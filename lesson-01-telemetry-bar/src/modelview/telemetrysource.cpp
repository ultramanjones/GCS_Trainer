#include "modelview/telemetrysource.h"

TelemetrySource::TelemetrySource(QObject *parent)
    : QObject(parent)
    , m_readingTimer(this)
    , m_publishTimer(this)
{
    m_readingTimer.setInterval(20);  // 50 Hz
    m_publishTimer.setInterval(50);  // 20 Hz

    connect(&m_readingTimer, &QTimer::timeout, this, &TelemetrySource::makeReading);
    connect(&m_publishTimer, &QTimer::timeout, this, &TelemetrySource::publishSnapshot);
}

void TelemetrySource::start()
{
    m_readingTimer.start();
    m_publishTimer.start();
}

void TelemetrySource::stopLink()
{
    m_readingTimer.stop();
    m_publishTimer.stop();
}

void TelemetrySource::makeReading()
{
    // TODO (Lesson 1, Step 2): fill this in.
    //
    // Make m_latestReading look like a real vehicle instead of one
    // fixed value. Keep it simple:
    //   - flightMode: leave as "Cruise" for now.
    //   - armed: true once m_tickCount passes some number of ticks.
    //   - gpsFixType / satelliteCount: pick fixed values, 3 and 9 are fine.
    //   - rssiPercent: swing it slowly, e.g. using m_tickCount.
    //   - batteryPercent: count down slowly from 100.
    //
    // This runs at 50 Hz. It only ever touches m_latestReading. It
    // never emits anything — that is publishSnapshot's job.
    ++m_tickCount;
}

void TelemetrySource::publishSnapshot()
{
    // TODO (Lesson 1, Step 2): emit snapshotReady with m_latestReading.
    //
    // That is the whole coalescing step: whatever makeReading() wrote
    // most recently is what goes out, once, right now. Nothing from
    // the 50 Hz side is ever lost in a meaningful way — the receiver
    // never needed 50 updates a second, only the most recent value.
    //
    // Be ready to say out loud: why does this timer run slower than
    // m_readingTimer? What would happen to the UI thread if this timer
    // ran at 50 Hz too?
}
