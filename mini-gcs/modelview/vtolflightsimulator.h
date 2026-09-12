#pragma once

#include <QString>

#include "model/telemetrysnapshot.h"

// A pretend tail-sitter drone.
//
// A tail-sitter is a drone that takes off straight up like a
// helicopter, then tips over and flies forward on wings like an
// airplane. Shield AI builds aircraft that do this, so the practice
// vehicle does it too.
//
// This class is plain C++. No Qt objects, no signals, no timers. You
// hand it a slice of time and it moves the aircraft forward by that
// much. That makes it easy to test with no window open, which is the
// point of keeping the rules down here in the back end.
class VtolFlightSimulator
{
public:
    VtolFlightSimulator();

    // Move the aircraft forward by this many seconds of flying.
    void advanceOneStep(double stepSeconds);

    // Try to carry out one order. Returns true if the order made
    // sense right now, false if it did not.
    bool tryToApplyCommand(const QString &commandName);

    // Everything the radio would report at this moment.
    TelemetrySnapshot currentTelemetrySnapshot() const;

private:
    // What part of the flight we are in.
    enum class FlightStage
    {
        Disarmed,
        ArmedOnGround,
        HoverClimb,
        TransitionToWing,
        WingBorneCircuit,
        ReturningHome,
        Landing
    };

    QString flightStageDisplayName() const;
    void advanceWingBorneCircuit(double stepSeconds);
    void advanceReturnToHome(double stepSeconds);

    FlightStage m_flightStage = FlightStage::Disarmed;

    double m_eastMetersFromHome = 0.0;
    double m_northMetersFromHome = 0.0;
    double m_altitudeMetersAboveHome = 0.0;

    double m_headingDegrees = 0.0;
    double m_rollDegrees = 0.0;
    double m_pitchDegrees = 0.0;

    double m_airspeedMetersPerSecond = 0.0;
    double m_groundspeedMetersPerSecond = 0.0;

    double m_batteryPercent = 100.0;
    double m_secondsFlown = 0.0;

    // Where we are around the circle, in radians.
    double m_circuitAngleRadians = 0.0;

    // How long we have been tipping over from hover to wing flight.
    double m_transitionSecondsElapsed = 0.0;

    // The launch point on the real globe. Everything else is measured
    // in meters east and north of this spot.
    static constexpr double kHomeLatitudeDegrees = 40.5062;
    static constexpr double kHomeLongitudeDegrees = -79.8556;

    static constexpr double kCruiseAltitudeMeters = 40.0;
    static constexpr double kCircuitRadiusMeters = 250.0;
    static constexpr double kCruiseSpeedMetersPerSecond = 22.0;
    static constexpr double kClimbRateMetersPerSecond = 3.0;
    static constexpr double kDescentRateMetersPerSecond = 2.0;
    static constexpr double kTransitionSeconds = 4.0;
};
