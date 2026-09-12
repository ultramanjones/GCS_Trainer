#include "modelview/racetrackflightpattern.h"

#include <QtMath>

#include "model/vehicleattitude.h"
#include "model/vehicleposition.h"

RacetrackFlightPattern::RacetrackFlightPattern()
{
}

QString RacetrackFlightPattern::patternName() const
{
    return QStringLiteral("Racetrack");
}

void RacetrackFlightPattern::beginFrom(const VehiclePosition &startingPosition,
                                       const VehicleAttitude &startingAttitude)
{
    // The racetrack starts from wherever the vehicle already is, on
    // whatever heading it already has. Nothing jumps.
    Q_UNUSED(startingPosition);
    Q_UNUSED(startingAttitude);

    m_currentLeg = RacetrackLeg::OutboundStraight;
    m_metersFlownOnLeg = 0.0;
    m_degreesTurnedOnLeg = 0.0;
    m_secondsFlown = 0.0;
}

bool RacetrackFlightPattern::isOnATurn() const
{
    return m_currentLeg == RacetrackLeg::FirstTurn
        || m_currentLeg == RacetrackLeg::SecondTurn;
}

void RacetrackFlightPattern::flyOneStep(double stepSeconds,
                                        double speedMetersPerSecond,
                                        VehiclePosition &position,
                                        VehicleAttitude &attitude)
{
    // THE RACETRACK, SPELLED OUT.
    //
    // Four legs, forever: a straight, a turn all the way around, the
    // straight back the other way, and a turn back to the start.
    //
    // On a straight leg the heading is left alone and the meters are
    // counted until the leg is long enough.
    //
    // On a turn the heading changes a little every step. How fast it
    // changes comes from one fact: an aircraft flying a circle goes
    // all the way around in the time it takes to fly the edge of that
    // circle. So the turn rate in radians per second is the speed
    // divided by the radius of the turn. Add that to the heading each
    // step and count the degrees until a hundred and eighty have gone
    // by. Moving forward along the new heading each step traces the
    // curve on its own. There is no circle math anywhere.
    //
    // The bank angle is the part that shows on the instrument. A
    // straight leg asks for wings level with a slow wander. A turn
    // asks for a real bank. The easing in VehicleAttitude does the
    // rest, so the horizon rolls in at the top of each turn and rolls
    // back out at the bottom.

    m_secondsFlown += stepSeconds;

    // A touch of nose up trim that breathes, so the instrument is
    // never perfectly still. Real aircraft never are.
    attitude.setTargetPitchDegrees(1.5 + (1.2 * qSin(m_secondsFlown * 0.7)));

    if (isOnATurn()) {
        const double turnRateDegreesPerSecond =
            qRadiansToDegrees(speedMetersPerSecond / kTurnRadiusMeters);
        const double degreesThisStep = turnRateDegreesPerSecond * stepSeconds;

        attitude.turnByDegrees(degreesThisStep);
        m_degreesTurnedOnLeg += degreesThisStep;

        attitude.setTargetRollDegrees(kTurnBankDegrees);

        if (m_degreesTurnedOnLeg >= kHalfTurnDegrees) {
            m_currentLeg = (m_currentLeg == RacetrackLeg::FirstTurn)
                         ? RacetrackLeg::InboundStraight
                         : RacetrackLeg::OutboundStraight;
            m_metersFlownOnLeg = 0.0;
            m_degreesTurnedOnLeg = 0.0;
        }
    } else {
        attitude.setTargetRollDegrees(2.0 * qSin(m_secondsFlown * 0.45));

        m_metersFlownOnLeg += speedMetersPerSecond * stepSeconds;

        if (m_metersFlownOnLeg >= kStraightLegMeters) {
            m_currentLeg = (m_currentLeg == RacetrackLeg::OutboundStraight)
                         ? RacetrackLeg::FirstTurn
                         : RacetrackLeg::SecondTurn;
            m_metersFlownOnLeg = 0.0;
            m_degreesTurnedOnLeg = 0.0;
        }
    }

    position.moveAlongHeading(attitude.headingDegrees(),
                              speedMetersPerSecond * stepSeconds);
}
