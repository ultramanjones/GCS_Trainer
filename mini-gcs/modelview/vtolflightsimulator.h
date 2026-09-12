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
        Landing,

        // The motors have been cut on purpose, in the air. This is not
        // a flight mode. It is the aircraft falling.
        MotorsCut
    };

    // Once it is flying on the wing, the aircraft goes around a
    // racetrack: up one side, turn around, down the other side, turn
    // around again. Real aircraft fly patterns like this, and it means
    // the wings roll into a turn and level out again instead of
    // sitting at one fixed angle forever.
    enum class CircuitLeg
    {
        OutboundStraight,
        FirstTurn,
        InboundStraight,
        SecondTurn
    };

    QString flightStageDisplayName() const;
    void advanceRacetrackCircuit(double stepSeconds);
    void advanceReturnToHome(double stepSeconds);
    void advanceFallWithMotorsCut(double stepSeconds);
    void moveForward(double stepSeconds);
    void easeTowardTargetAttitude(double stepSeconds);

    FlightStage m_flightStage = FlightStage::Disarmed;
    CircuitLeg m_circuitLeg = CircuitLeg::OutboundStraight;

    double m_eastMetersFromHome = 0.0;
    double m_northMetersFromHome = 0.0;
    double m_altitudeMetersAboveHome = 0.0;

    double m_headingDegrees = 0.0;

    // Where the wings are right now, and where they are heading.
    // Nothing snaps. Every stage sets a target and one function eases
    // the real value toward it, so the instrument always moves the way
    // an aircraft moves.
    double m_rollDegrees = 0.0;
    double m_pitchDegrees = 0.0;
    double m_targetRollDegrees = 0.0;
    double m_targetPitchDegrees = 0.0;

    double m_airspeedMetersPerSecond = 0.0;
    double m_groundspeedMetersPerSecond = 0.0;

    // True once the aircraft has hit the ground with the motors off.
    // It stays true until somebody arms it again, so the flight mode
    // reads "Crashed" instead of a tidy "Disarmed".
    bool m_hasCrashed = false;

    // How fast the aircraft is falling once the motors are off.
    double m_fallSpeedMetersPerSecond = 0.0;

    double m_batteryPercent = 100.0;
    double m_secondsFlown = 0.0;

    // How far along the current straight leg, in meters.
    double m_metersFlownOnLeg = 0.0;

    // How far around the current turn, in degrees. A turn is finished
    // at a hundred and eighty.
    double m_degreesTurnedOnLeg = 0.0;

    // How long we have been tipping over from hover to wing flight.
    double m_transitionSecondsElapsed = 0.0;

    // The launch point on the real globe. Everything else is measured
    // in meters east and north of this spot.
    static constexpr double kHomeLatitudeDegrees = 40.5062;
    static constexpr double kHomeLongitudeDegrees = -79.8556;

    static constexpr double kCruiseAltitudeMeters = 40.0;
    static constexpr double kCruiseSpeedMetersPerSecond = 26.0;
    static constexpr double kClimbRateMetersPerSecond = 3.0;
    static constexpr double kDescentRateMetersPerSecond = 2.0;
    static constexpr double kTransitionSeconds = 4.0;

    static constexpr double kStraightLegMeters = 220.0;
    static constexpr double kTurnRadiusMeters = 110.0;
    static constexpr double kTurnBankDegrees = 22.0;
    static constexpr double kReturnBankDegrees = 8.0;

    // Gravity, and how fast a tumbling airframe stops speeding up.
    static constexpr double kGravityMetersPerSecondSquared = 9.81;
    static constexpr double kTerminalFallSpeedMetersPerSecond = 26.0;

    // How fast the wings catch up to where they are supposed to be.
    // Bigger is snappier. Two and a half takes about a second and a
    // half to settle, which is what a real roll-in looks like.
    static constexpr double kAttitudeEaseRatePerSecond = 2.5;
};
