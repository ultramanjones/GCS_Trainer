#pragma once

#include <QMetaType>
#include <QString>

// One full set of vehicle readings, taken at one moment.
//
// This is a plain data holder. No functions that do work. No pointer
// to anything else. It is the cargo that travels from the fake radio
// link up to the screen, and it is the only thing that crosses the
// thread line.
//
// There is no database in this program, so the model layer is just
// these records. If real storage ever showed up, it would go here and
// nowhere else.
struct TelemetrySnapshot
{
    // What the vehicle is doing right now, in words for the screen.
    QString flightModeName;
    bool isArmed = false;

    // 0 means no fix, 2 means a flat two-dimensional fix, 3 means a
    // full three-dimensional fix. These numbers match the ones a real
    // MAVLink radio sends, so they line up with the real thing.
    int gpsFixType = 0;
    int satelliteCount = 0;

    int radioSignalPercent = 0;
    int batteryPercent = 100;

    // Where the vehicle is, given two ways on purpose.
    //
    // Degrees are what a real radio reports and what a person reads.
    // Meters east and north of the launch point are what the map
    // drawing needs. Both are worked out down here, once, so no layer
    // above ever has to do map math.
    double latitudeDegrees = 0.0;
    double longitudeDegrees = 0.0;
    double eastMetersFromHome = 0.0;
    double northMetersFromHome = 0.0;
    double altitudeMetersAboveHome = 0.0;

    // How the vehicle is sitting in the air.
    double rollDegrees = 0.0;
    double pitchDegrees = 0.0;
    double headingDegrees = 0.0;

    double airspeedMetersPerSecond = 0.0;
    double groundspeedMetersPerSecond = 0.0;
};

// This line lets Qt carry a TelemetrySnapshot through a queued
// signal between two threads. Without it the snapshot never arrives
// and you get a console warning instead of data.
Q_DECLARE_METATYPE(TelemetrySnapshot)
