#pragma once

#include <QMetaType>
#include <QString>

#include "model/vehicleattitude.h"
#include "model/vehiclebattery.h"
#include "model/vehicleposition.h"

// One situation report from a vehicle.
//
// A sitrep is a letter, not an object that owns anything. It is what a
// vehicle hands to a radio, and what a radio hands to a ground
// station. Its shape is set by what the ground station needs to know,
// and nothing else.
//
// It is built out of the same value types the vehicle itself uses, so
// nothing has to be taken apart and put back together on the way.
//
// QGroundControl does not have one class like this. It wraps every
// value in a Fact carrying its own units and range, and groups those
// into FactGroups.
struct VehicleSitRep
{
    // Which vehicle this came from. One radio can carry several.
    int vehicleIdentifier = 0;

    QString flightModeName;
    bool isArmed = false;

    // 0 means no fix, 2 means a flat two dimensional fix, 3 means a
    // full three dimensional fix. These match what a real MAVLink
    // radio sends.
    int gpsFixType = 0;
    int satelliteCount = 0;

    // How strong the vehicle says the radio signal is where it is.
    int radioSignalPercent = 0;

    VehiclePosition position;
    VehicleAttitude attitude;
    VehicleBattery battery;

    double airspeedMetersPerSecond = 0.0;
    double groundspeedMetersPerSecond = 0.0;
};

// Lets Qt carry a sitrep through a queued signal between two threads.
// Without this the signal fires, the slot never runs, and you get one
// console warning to explain why.
Q_DECLARE_METATYPE(VehicleSitRep)
