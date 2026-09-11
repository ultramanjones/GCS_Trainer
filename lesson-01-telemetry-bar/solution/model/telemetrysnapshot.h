#pragma once

#include <QMetaType>
#include <QString>

// TelemetrySnapshot is one full set of vehicle status values, taken at
// one moment in time. It is a plain data holder: no functions that do
// work, and no pointer to anything else. This is the cargo that
// travels from the worker thread up to the screen.
struct TelemetrySnapshot
{
    QString flightMode;
    bool armed = false;

    // 0 = no GPS fix, 2 = 2D fix, 3 = 3D fix. This matches MAVLink's
    // GPS_FIX_TYPE enum, so it lines up with what a real link sends.
    int gpsFixType = 0;
    int satelliteCount = 0;

    int rssiPercent = 0;
    int batteryPercent = 100;
};

// Q_DECLARE_METATYPE lets Qt carry a TelemetrySnapshot value through a
// queued signal-slot connection between threads. Without this line,
// connecting TelemetrySource::snapshotReady to a slot with
// Qt::QueuedConnection fails at runtime — Qt cannot copy an unknown
// type into its cross-thread event queue, so the slot never runs and
// you only get a console warning to explain why.
Q_DECLARE_METATYPE(TelemetrySnapshot)
