#pragma once

#include <QString>
#include <memory>

#include "model/vehicleattitude.h"
#include "model/vehiclebattery.h"
#include "model/vehiclecommand.h"
#include "model/vehicleposition.h"
#include "model/vehiclesitrep.h"
#include "modelview/flightpattern.h"

// A pretend tail-sitter drone.
//
// A tail-sitter takes off straight up like a helicopter, then tips
// over and flies forward on wings like an airplane. Shield AI builds
// aircraft that do this.
//
// This object is the aircraft. It does not exist in a real deployment
// — there it is a physical machine in a field somewhere, and the only
// thing the ground has is a MapVehicle built from messages. Nothing
// on the ground is allowed to hold one of these.
//
// It is plain C++. No Qt objects, no signals, no timers. Hand it a
// slice of time and it flies for that long. That means it can be
// tested with no window open.
class SimulatedVehicle
{
public:
    // What the aircraft is doing. The aircraft HAS a mode, so the
    // rules for changing it live here and nowhere else.
    enum class FlightMode
    {
        Disarmed,
        Crashed,
        ArmedOnGround,
        HoverClimb,
        TransitionToWing,
        WingBorne,
        ReturningHome,
        Landing,
        MotorsCut
    };

    explicit SimulatedVehicle(int vehicleIdentifier);

    int vehicleIdentifier() const;
    FlightMode flightMode() const;
    QString flightModeName() const;
    bool isArmed() const;

    // Answer an order from the ground. Every order gets an answer,
    // and a refused answer says why in words the operator can read.
    VehicleCommandAcknowledgment answerCommand(const VehicleCommandRequest &request);

    // Fly for this long.
    void advanceOneStep(double stepSeconds);

    // Everything this aircraft would report right now.
    VehicleSitRep currentSitRep() const;

private:
    // THE ONE DOOR. This is the only function in the program that
    // changes the flight mode. Everything that wants a mode change
    // asks here, and the rulebook below decides.
    bool tryToChangeModeTo(FlightMode wantedMode, QString *refusalReason);

    // The rulebook. Every legal move in the whole program, in one
    // place, readable top to bottom.
    bool isChangeAllowed(FlightMode fromMode, FlightMode toMode) const;

    static QString nameForMode(FlightMode mode);
    static FlightMode modeWantedByCommand(const QString &commandName, bool *wasUnderstood);

    void flyHoverClimb(double stepSeconds);
    void flyTransitionToWing(double stepSeconds);
    void flyWingBorne(double stepSeconds);
    void flyReturnToHome(double stepSeconds);
    void flyLanding(double stepSeconds);
    void fallWithMotorsCut(double stepSeconds);
    void drainBatteryFor(double stepSeconds);

    int m_vehicleIdentifier = 0;
    FlightMode m_flightMode = FlightMode::Disarmed;

    VehiclePosition m_position;
    VehicleAttitude m_attitude;
    VehicleBattery m_battery;

    std::unique_ptr<FlightPattern> m_flightPattern;

    double m_airspeedMetersPerSecond = 0.0;
    double m_groundspeedMetersPerSecond = 0.0;
    double m_fallSpeedMetersPerSecond = 0.0;
    double m_transitionSecondsElapsed = 0.0;
    double m_secondsFlown = 0.0;

    static constexpr double kCruiseAltitudeMeters = 40.0;
    static constexpr double kCruiseSpeedMetersPerSecond = 26.0;
    static constexpr double kClimbRateMetersPerSecond = 3.0;
    static constexpr double kDescentRateMetersPerSecond = 2.0;
    static constexpr double kTransitionSeconds = 4.0;
    static constexpr double kTurnRadiusMeters = 110.0;
    static constexpr double kReturnBankDegrees = 8.0;
    static constexpr double kCloseEnoughToHomeMeters = 15.0;

    static constexpr double kAttitudeEaseRatePerSecond = 2.5;
    static constexpr double kGravityMetersPerSecondSquared = 9.81;
    static constexpr double kTerminalFallSpeedMetersPerSecond = 26.0;

    static constexpr double kHoveringDrainPercentPerSecond = 0.20;
    static constexpr double kWingBorneDrainPercentPerSecond = 0.08;
};
