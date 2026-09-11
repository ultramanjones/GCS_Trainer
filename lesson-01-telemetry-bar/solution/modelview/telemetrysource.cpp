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
    // Runs at 50 Hz. Only ever touches m_latestReading. Never emits
    // anything — that is publishSnapshot's job.
    ++m_tickCount;

    m_latestReading.flightMode = QStringLiteral("Cruise");
    m_latestReading.armed = m_tickCount > 100;  // arms after ~2 s
    m_latestReading.gpsFixType = 3;
    m_latestReading.satelliteCount = 9;

    // Swing RSSI slowly between 60 and 100 using a triangle wave.
    const int rssiPhase = m_tickCount % 200;
    m_latestReading.rssiPercent = 60 + (rssiPhase < 100 ? rssiPhase : 200 - rssiPhase) / 2;

    // Count battery down slowly from 100, wrapping for demo purposes.
    m_latestReading.batteryPercent = 100 - (m_tickCount / 50) % 100;
}

void TelemetrySource::publishSnapshot()
{
    // This is the whole coalescing step: whatever makeReading() wrote
    // most recently is what goes out, once, right now. This timer
    // runs at 20 Hz instead of 50 Hz because the screen never needed
    // 50 updates a second — only the most recent value, often enough
    // to look smooth. Publishing at 50 Hz would mean 50 cross-thread
    // queued calls and 50 QML property updates a second for values a
    // person cannot perceive changing that fast; it wastes CPU and
    // can make the UI thread's event queue back up under load.
    emit snapshotReady(m_latestReading);
}
