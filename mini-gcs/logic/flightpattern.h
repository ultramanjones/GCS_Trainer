#pragma once

#include <QString>

class VehicleAttitude;
class VehiclePosition;

// Where to fly.
//
// A pattern knows where the vehicle should go. It does not know how to
// fly, and it does not own the vehicle. This split is how the real
// thing works: the autopilot flies the aircraft, the mission tells the
// autopilot where to go.
//
// Because this is its own class, a new pattern is a new class. It is
// never an edit to the vehicle.
class FlightPattern
{
public:
    virtual ~FlightPattern() = default;

    virtual QString patternName() const = 0;

    // Start the pattern from wherever the vehicle is right now.
    virtual void beginFrom(const VehiclePosition &startingPosition,
                           const VehicleAttitude &startingAttitude) = 0;

    // Fly one step of the pattern. The pattern turns the vehicle and
    // moves it, and says how hard the wings should be banked while it
    // does. It never touches anything else about the vehicle.
    virtual void flyOneStep(double stepSeconds,
                            double speedMetersPerSecond,
                            VehiclePosition &position,
                            VehicleAttitude &attitude) = 0;
};
