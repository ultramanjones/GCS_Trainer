#include "modelview/vtolflightsimulator.h"

#include <QtMath>

namespace {

// Keeps an angle in the range zero up to three hundred sixty.
double wrappedHeadingDegrees(double headingDegrees)
{
    while (headingDegrees < 0.0)
        headingDegrees += 360.0;
    while (headingDegrees >= 360.0)
        headingDegrees -= 360.0;
    return headingDegrees;
}

}  // namespace

VtolFlightSimulator::VtolFlightSimulator()
{
}

QString VtolFlightSimulator::flightStageDisplayName() const
{
    if (m_flightStage == FlightStage::Disarmed && m_hasCrashed)
        return QStringLiteral("Crashed");

    switch (m_flightStage) {
    case FlightStage::Disarmed:         return QStringLiteral("Disarmed");
    case FlightStage::ArmedOnGround:    return QStringLiteral("Armed");
    case FlightStage::HoverClimb:       return QStringLiteral("Hover");
    case FlightStage::TransitionToWing: return QStringLiteral("Transition");
    case FlightStage::WingBorneCircuit: return QStringLiteral("Cruise");
    case FlightStage::ReturningHome:    return QStringLiteral("Return");
    case FlightStage::Landing:          return QStringLiteral("Land");
    case FlightStage::MotorsCut:        return QStringLiteral("MOTORS CUT");
    }
    return QStringLiteral("Unknown");
}

bool VtolFlightSimulator::tryToApplyCommand(const QString &commandName)
{
    if (commandName == QLatin1String("Arm")) {
        if (m_flightStage != FlightStage::Disarmed)
            return false;
        m_flightStage = FlightStage::ArmedOnGround;

        // Arming is what clears a crash. In a real program you would
        // walk out and pick the aircraft up first.
        m_hasCrashed = false;
        return true;
    }

    if (commandName == QLatin1String("Disarm")) {
        // Only safe on the ground.
        if (m_flightStage != FlightStage::ArmedOnGround)
            return false;
        m_flightStage = FlightStage::Disarmed;
        return true;
    }

    if (commandName == QLatin1String("Launch")) {
        if (m_flightStage != FlightStage::ArmedOnGround)
            return false;
        m_flightStage = FlightStage::HoverClimb;
        return true;
    }

    if (commandName == QLatin1String("Return")) {
        const bool isFlying = m_flightStage == FlightStage::HoverClimb
                           || m_flightStage == FlightStage::TransitionToWing
                           || m_flightStage == FlightStage::WingBorneCircuit;
        if (!isFlying)
            return false;
        m_flightStage = FlightStage::ReturningHome;
        return true;
    }

    if (commandName == QLatin1String("EmergencyStop")) {
        // This one is allowed whenever the motors are turning, in the
        // air or on the ground. That is the whole point of it. It is
        // never refused, because the moment you need it is exactly the
        // moment a refusal would be unforgivable.
        if (m_flightStage == FlightStage::Disarmed
            || m_flightStage == FlightStage::MotorsCut)
            return false;
        m_flightStage = FlightStage::MotorsCut;
        m_fallSpeedMetersPerSecond = 0.0;
        return true;
    }

    if (commandName == QLatin1String("Land")) {
        const bool isFlying = m_flightStage == FlightStage::HoverClimb
                           || m_flightStage == FlightStage::TransitionToWing
                           || m_flightStage == FlightStage::WingBorneCircuit
                           || m_flightStage == FlightStage::ReturningHome;
        if (!isFlying)
            return false;
        m_flightStage = FlightStage::Landing;
        return true;
    }

    return false;
}

void VtolFlightSimulator::moveForward(double stepSeconds)
{
    // Take one step in whatever direction the nose is pointing.
    //
    // A heading of zero means north, and north is the positive north
    // axis. A heading of ninety means east. That is why east uses the
    // sine and north uses the cosine, which is the opposite of the way
    // angles are usually written in a math class.
    const double headingRadians = qDegreesToRadians(m_headingDegrees);
    const double stepMeters = m_groundspeedMetersPerSecond * stepSeconds;

    m_eastMetersFromHome += qSin(headingRadians) * stepMeters;
    m_northMetersFromHome += qCos(headingRadians) * stepMeters;
}

void VtolFlightSimulator::easeTowardTargetAttitude(double stepSeconds)
{
    // Move the wings part of the way toward where they are supposed to
    // be, every step, instead of snapping straight there.
    //
    // The fraction moved each step is the ease rate times the step
    // length. It is held at one so a long step can never overshoot and
    // start swinging. This is the same one-line smoothing every flight
    // instrument uses, and it is what makes a roll-in look like a
    // roll-in instead of a jump cut.
    const double fractionOfTheWay = qMin(1.0, kAttitudeEaseRatePerSecond * stepSeconds);

    m_rollDegrees += (m_targetRollDegrees - m_rollDegrees) * fractionOfTheWay;
    m_pitchDegrees += (m_targetPitchDegrees - m_pitchDegrees) * fractionOfTheWay;
}

void VtolFlightSimulator::advanceOneStep(double stepSeconds)
{
    if (m_flightStage != FlightStage::Disarmed)
        m_secondsFlown += stepSeconds;

    switch (m_flightStage) {

    case FlightStage::Disarmed:
    case FlightStage::ArmedOnGround:
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_targetRollDegrees = 0.0;
        m_targetPitchDegrees = 0.0;
        break;

    case FlightStage::HoverClimb:
        // Straight up, nose level, no forward speed. A tail-sitter
        // hovering looks like a helicopter to the numbers. It rocks a
        // little, the way a hovering aircraft holding position does.
        m_altitudeMetersAboveHome += kClimbRateMetersPerSecond * stepSeconds;
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_targetRollDegrees = 3.0 * qSin(m_secondsFlown * 0.9);
        m_targetPitchDegrees = 2.0 * qSin(m_secondsFlown * 0.6);
        if (m_altitudeMetersAboveHome >= kCruiseAltitudeMeters) {
            m_altitudeMetersAboveHome = kCruiseAltitudeMeters;
            m_flightStage = FlightStage::TransitionToWing;
            m_transitionSecondsElapsed = 0.0;
        }
        break;

    case FlightStage::TransitionToWing: {
        // Tipping over. Speed builds from nothing to cruise speed over
        // a few seconds, and the nose drops while it happens.
        m_transitionSecondsElapsed += stepSeconds;
        const double howFarAlong =
            qBound(0.0, m_transitionSecondsElapsed / kTransitionSeconds, 1.0);

        m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond * howFarAlong;
        m_groundspeedMetersPerSecond = m_airspeedMetersPerSecond;
        m_targetPitchDegrees = -11.0 * howFarAlong;
        m_targetRollDegrees = 0.0;

        moveForward(stepSeconds);

        if (m_transitionSecondsElapsed >= kTransitionSeconds) {
            // Carry straight on into the first leg of the racetrack
            // from exactly where we are. Nothing jumps.
            m_flightStage = FlightStage::WingBorneCircuit;
            m_circuitLeg = CircuitLeg::OutboundStraight;
            m_metersFlownOnLeg = 0.0;
            m_degreesTurnedOnLeg = 0.0;
        }
        break;
    }

    case FlightStage::WingBorneCircuit:
        advanceRacetrackCircuit(stepSeconds);
        break;

    case FlightStage::MotorsCut:
        advanceFallWithMotorsCut(stepSeconds);
        break;

    case FlightStage::ReturningHome:
        advanceReturnToHome(stepSeconds);
        break;

    case FlightStage::Landing:
        // Straight down, wherever we are.
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_targetRollDegrees = 0.0;
        m_targetPitchDegrees = 0.0;
        m_altitudeMetersAboveHome -= kDescentRateMetersPerSecond * stepSeconds;
        if (m_altitudeMetersAboveHome <= 0.0) {
            m_altitudeMetersAboveHome = 0.0;
            m_flightStage = FlightStage::Disarmed;
        }
        break;
    }

    easeTowardTargetAttitude(stepSeconds);

    // Battery drains faster when the motors are holding the whole
    // aircraft up. Hovering costs far more than flying on the wing,
    // which is the entire reason a tail-sitter tips over at all.
    if (m_flightStage == FlightStage::HoverClimb
        || m_flightStage == FlightStage::TransitionToWing
        || m_flightStage == FlightStage::Landing) {
        m_batteryPercent -= 0.20 * stepSeconds;
    } else if (m_flightStage != FlightStage::Disarmed
               && m_flightStage != FlightStage::MotorsCut) {
        m_batteryPercent -= 0.08 * stepSeconds;
    }
    // Nothing drains while the motors are off. That is what off means.
    m_batteryPercent = qBound(0.0, m_batteryPercent, 100.0);
}

void VtolFlightSimulator::advanceRacetrackCircuit(double stepSeconds)
{
    // THE RACETRACK, spelled out.
    //
    // Four legs, forever: a straight, a turn all the way around, the
    // straight back the other way, and a turn back to the start.
    //
    // On a straight leg the heading is left alone and we count meters
    // until the leg is long enough.
    //
    // In a turn the heading changes a little every step. How fast it
    // changes comes from one fact: an aircraft flying a circle turns
    // through its whole circle in the time it takes to fly around the
    // edge. So turn rate in radians per second is speed divided by the
    // radius of the turn. We add that to the heading each step and
    // count the degrees until we have come around a hundred and
    // eighty. Moving forward along the new heading each step traces
    // the curve on its own, with no circle math anywhere.
    //
    // Rolling is the part that matters on the instrument. A straight
    // leg asks for wings level with a slow easy wander. A turn asks
    // for a real bank. The easing function does the rest, so the
    // horizon rolls in at the top of each turn and rolls out at the
    // bottom.

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_altitudeMetersAboveHome = kCruiseAltitudeMeters;

    // A touch of nose-up trim that breathes, so the instrument is
    // never perfectly still. Real aircraft never are.
    m_targetPitchDegrees = 1.5 + 1.2 * qSin(m_secondsFlown * 0.7);

    const bool isTurning = m_circuitLeg == CircuitLeg::FirstTurn
                        || m_circuitLeg == CircuitLeg::SecondTurn;

    if (isTurning) {
        const double turnRateDegreesPerSecond =
            qRadiansToDegrees(kCruiseSpeedMetersPerSecond / kTurnRadiusMeters);
        const double degreesThisStep = turnRateDegreesPerSecond * stepSeconds;

        m_headingDegrees = wrappedHeadingDegrees(m_headingDegrees + degreesThisStep);
        m_degreesTurnedOnLeg += degreesThisStep;

        m_targetRollDegrees = kTurnBankDegrees;

        if (m_degreesTurnedOnLeg >= 180.0) {
            m_circuitLeg = (m_circuitLeg == CircuitLeg::FirstTurn)
                         ? CircuitLeg::InboundStraight
                         : CircuitLeg::OutboundStraight;
            m_metersFlownOnLeg = 0.0;
            m_degreesTurnedOnLeg = 0.0;
        }
    } else {
        m_targetRollDegrees = 2.0 * qSin(m_secondsFlown * 0.45);

        m_metersFlownOnLeg += kCruiseSpeedMetersPerSecond * stepSeconds;

        if (m_metersFlownOnLeg >= kStraightLegMeters) {
            m_circuitLeg = (m_circuitLeg == CircuitLeg::OutboundStraight)
                         ? CircuitLeg::FirstTurn
                         : CircuitLeg::SecondTurn;
            m_metersFlownOnLeg = 0.0;
            m_degreesTurnedOnLeg = 0.0;
        }
    }

    moveForward(stepSeconds);
}

void VtolFlightSimulator::advanceReturnToHome(double stepSeconds)
{
    // Point at the launch point and fly to it. When close, stop and
    // land.
    const double eastToGo = -m_eastMetersFromHome;
    const double northToGo = -m_northMetersFromHome;
    const double distanceMeters = qSqrt(eastToGo * eastToGo + northToGo * northToGo);

    if (distanceMeters < 15.0) {
        m_flightStage = FlightStage::Landing;
        return;
    }

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_altitudeMetersAboveHome = kCruiseAltitudeMeters;
    m_targetPitchDegrees = 1.0;

    // Turn toward home instead of snapping the nose around. Work out
    // the heading we want, then how far off we are, kept in the range
    // minus a hundred eighty to plus a hundred eighty so the aircraft
    // always turns the short way around. Bank into whichever side the
    // turn is on, and stop banking once we are lined up.
    const double wantedHeadingDegrees =
        wrappedHeadingDegrees(qRadiansToDegrees(qAtan2(eastToGo, northToGo)));

    double headingErrorDegrees = wantedHeadingDegrees - m_headingDegrees;
    while (headingErrorDegrees > 180.0)
        headingErrorDegrees -= 360.0;
    while (headingErrorDegrees < -180.0)
        headingErrorDegrees += 360.0;

    const double turnRateDegreesPerSecond =
        qRadiansToDegrees(kCruiseSpeedMetersPerSecond / kTurnRadiusMeters);
    const double mostWeCanTurnThisStep = turnRateDegreesPerSecond * stepSeconds;
    const double turnThisStep =
        qBound(-mostWeCanTurnThisStep, headingErrorDegrees, mostWeCanTurnThisStep);

    m_headingDegrees = wrappedHeadingDegrees(m_headingDegrees + turnThisStep);

    if (qAbs(headingErrorDegrees) < 2.0)
        m_targetRollDegrees = 0.0;
    else
        m_targetRollDegrees = (headingErrorDegrees > 0.0) ? kReturnBankDegrees
                                                          : -kReturnBankDegrees;

    moveForward(stepSeconds);
}

void VtolFlightSimulator::advanceFallWithMotorsCut(double stepSeconds)
{
    // No motors. No control. Just gravity and whatever speed it had.
    //
    // Falling speed builds at gravity until the airframe is tumbling
    // hard enough to stop it building. Forward speed bleeds off with
    // no thrust pushing it. The wings swing because nothing is holding
    // them anywhere.
    m_fallSpeedMetersPerSecond =
        qMin(kTerminalFallSpeedMetersPerSecond,
             m_fallSpeedMetersPerSecond + kGravityMetersPerSecondSquared * stepSeconds);

    m_altitudeMetersAboveHome -= m_fallSpeedMetersPerSecond * stepSeconds;

    m_groundspeedMetersPerSecond = qMax(0.0, m_groundspeedMetersPerSecond - 5.0 * stepSeconds);
    m_airspeedMetersPerSecond = m_groundspeedMetersPerSecond;
    moveForward(stepSeconds);

    m_targetRollDegrees = 55.0 * qSin(m_secondsFlown * 2.4);
    m_targetPitchDegrees = -38.0;

    if (m_altitudeMetersAboveHome <= 0.0) {
        m_altitudeMetersAboveHome = 0.0;
        m_fallSpeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_airspeedMetersPerSecond = 0.0;
        m_flightStage = FlightStage::Disarmed;

        // It did not land. It fell. The flight mode says so until
        // somebody arms it again.
        m_hasCrashed = true;
    }
}

TelemetrySnapshot VtolFlightSimulator::currentTelemetrySnapshot() const
{
    TelemetrySnapshot snapshot;

    snapshot.flightModeName = flightStageDisplayName();
    // Motors off means not armed, and the bar should say so the instant
    // it happens, while the aircraft is still in the air falling.
    snapshot.isArmed = m_flightStage != FlightStage::Disarmed
                    && m_flightStage != FlightStage::MotorsCut;

    snapshot.gpsFixType = 3;

    // Satellite count wanders a little so the screen looks alive.
    snapshot.satelliteCount = 11 + static_cast<int>(qFloor(2.0 * qAbs(qSin(m_secondsFlown / 7.0))));

    // Radio gets weaker the farther away the aircraft is.
    const double distanceMeters = qSqrt(m_eastMetersFromHome * m_eastMetersFromHome
                                      + m_northMetersFromHome * m_northMetersFromHome);
    snapshot.radioSignalPercent =
        static_cast<int>(qBound(0.0, 100.0 - (distanceMeters / 12.0), 100.0));

    snapshot.batteryPercent = static_cast<int>(qRound(m_batteryPercent));

    snapshot.eastMetersFromHome = m_eastMetersFromHome;
    snapshot.northMetersFromHome = m_northMetersFromHome;
    snapshot.altitudeMetersAboveHome = m_altitudeMetersAboveHome;

    // Turn meters into degrees on the globe.
    //
    // One degree of latitude is about 111320 meters everywhere. One
    // degree of longitude is that same number shrunk by the cosine of
    // the latitude, because the lines of longitude squeeze together as
    // you go north. This is the flat-earth shortcut. It is wrong over
    // hundreds of miles and exact enough over a few thousand feet,
    // which is all this practice vehicle ever flies.
    const double metersPerDegreeLatitude = 111320.0;
    const double metersPerDegreeLongitude =
        metersPerDegreeLatitude * qCos(qDegreesToRadians(kHomeLatitudeDegrees));

    snapshot.latitudeDegrees = kHomeLatitudeDegrees + (m_northMetersFromHome / metersPerDegreeLatitude);
    snapshot.longitudeDegrees = kHomeLongitudeDegrees + (m_eastMetersFromHome / metersPerDegreeLongitude);

    snapshot.rollDegrees = m_rollDegrees;
    snapshot.pitchDegrees = m_pitchDegrees;
    snapshot.headingDegrees = m_headingDegrees;

    snapshot.airspeedMetersPerSecond = m_airspeedMetersPerSecond;
    snapshot.groundspeedMetersPerSecond = m_groundspeedMetersPerSecond;

    return snapshot;
}
