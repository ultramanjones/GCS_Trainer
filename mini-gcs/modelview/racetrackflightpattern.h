#pragma once

#include "modelview/flightpattern.h"

// A racetrack: up one side, turn all the way around, down the other
// side, turn around again, forever.
//
// Real aircraft fly patterns like this to hold over an area. It also
// means the wings roll into a turn and level out again instead of
// sitting at one fixed angle, which a circle would do.
class RacetrackFlightPattern : public FlightPattern
{
public:
    RacetrackFlightPattern();

    QString patternName() const override;

    void beginFrom(const VehiclePosition &startingPosition,
                   const VehicleAttitude &startingAttitude) override;

    void flyOneStep(double stepSeconds,
                    double speedMetersPerSecond,
                    VehiclePosition &position,
                    VehicleAttitude &attitude) override;

private:
    enum class RacetrackLeg
    {
        OutboundStraight,
        FirstTurn,
        InboundStraight,
        SecondTurn
    };

    bool isOnATurn() const;

    RacetrackLeg m_currentLeg = RacetrackLeg::OutboundStraight;

    double m_metersFlownOnLeg = 0.0;
    double m_degreesTurnedOnLeg = 0.0;
    double m_secondsFlown = 0.0;

    static constexpr double kStraightLegMeters = 220.0;
    static constexpr double kTurnRadiusMeters = 110.0;
    static constexpr double kTurnBankDegrees = 22.0;
    static constexpr double kHalfTurnDegrees = 180.0;
};
