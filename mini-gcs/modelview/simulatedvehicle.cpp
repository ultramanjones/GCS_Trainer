#include "modelview/simulatedvehicle.h"

#include <QtMath>

#include "modelview/racetrackflightpattern.h"

SimulatedVehicle::SimulatedVehicle(int vehicleIdentifier)
    : m_vehicleIdentifier(vehicleIdentifier)
    , m_flightPattern(std::make_unique<RacetrackFlightPattern>())
{
}

int SimulatedVehicle::vehicleIdentifier() const { return m_vehicleIdentifier; }
SimulatedVehicle::FlightMode SimulatedVehicle::flightMode() const { return m_flightMode; }
QString SimulatedVehicle::flightModeName() const { return nameForMode(m_flightMode); }

bool SimulatedVehicle::isArmed() const
{
    // Motors off means not armed. Motors cut counts as not armed the
    // instant it happens, while the aircraft is still in the air.
    return m_flightMode != FlightMode::Disarmed
        && m_flightMode != FlightMode::Crashed
        && m_flightMode != FlightMode::MotorsCut;
}

QString SimulatedVehicle::nameForMode(FlightMode mode)
{
    switch (mode) {
    case FlightMode::Disarmed:         return QStringLiteral("Disarmed");
    case FlightMode::Crashed:          return QStringLiteral("Crashed");
    case FlightMode::ArmedOnGround:    return QStringLiteral("Armed");
    case FlightMode::HoverClimb:       return QStringLiteral("Hover");
    case FlightMode::TransitionToWing: return QStringLiteral("Transition");
    case FlightMode::WingBorne:        return QStringLiteral("Cruise");
    case FlightMode::ReturningHome:    return QStringLiteral("Return");
    case FlightMode::Landing:          return QStringLiteral("Land");
    case FlightMode::MotorsCut:        return QStringLiteral("MOTORS CUT");
    }
    return QStringLiteral("Unknown");
}

// ---------------------------------------------------------------------
// THE RULEBOOK
// ---------------------------------------------------------------------
bool SimulatedVehicle::isChangeAllowed(FlightMode fromMode, FlightMode toMode) const
{
    // Cutting the motors is allowed from anywhere the motors are
    // turning. This is first on purpose. The moment you need it is
    // exactly the moment a refusal would be unforgivable.
    if (toMode == FlightMode::MotorsCut)
        return isArmed();

    switch (fromMode) {

    // On the ground with the motors off. Arming is the only way out.
    // A crashed aircraft arms the same way, which stands in for
    // walking out and picking it up.
    case FlightMode::Disarmed:
    case FlightMode::Crashed:
        return toMode == FlightMode::ArmedOnGround;

    // Motors turning, still on the ground.
    case FlightMode::ArmedOnGround:
        return toMode == FlightMode::Disarmed
            || toMode == FlightMode::HoverClimb;

    // Climbing straight up. Tipping over is automatic at altitude.
    case FlightMode::HoverClimb:
        return toMode == FlightMode::TransitionToWing
            || toMode == FlightMode::ReturningHome
            || toMode == FlightMode::Landing;

    // Tipping over. Reaching wing-borne flight is automatic.
    case FlightMode::TransitionToWing:
        return toMode == FlightMode::WingBorne
            || toMode == FlightMode::ReturningHome
            || toMode == FlightMode::Landing;

    // Flying on the wing.
    case FlightMode::WingBorne:
        return toMode == FlightMode::ReturningHome
            || toMode == FlightMode::Landing;

    // Heading home. Landing happens on arrival.
    case FlightMode::ReturningHome:
        return toMode == FlightMode::Landing;

    // Coming down. Touchdown disarms it.
    case FlightMode::Landing:
        return toMode == FlightMode::Disarmed;

    // Falling. The ground ends it.
    case FlightMode::MotorsCut:
        return toMode == FlightMode::Crashed;
    }

    return false;
}

bool SimulatedVehicle::tryToChangeModeTo(FlightMode wantedMode, QString *refusalReason)
{
    if (wantedMode == m_flightMode) {
        if (refusalReason)
            *refusalReason = QStringLiteral("already in %1").arg(nameForMode(wantedMode));
        return false;
    }

    if (!isChangeAllowed(m_flightMode, wantedMode)) {
        if (refusalReason) {
            *refusalReason = QStringLiteral("cannot go from %1 to %2")
                                 .arg(nameForMode(m_flightMode), nameForMode(wantedMode));
        }
        return false;
    }

    m_flightMode = wantedMode;

    // Entering a mode sets up whatever that mode needs.
    switch (m_flightMode) {
    case FlightMode::TransitionToWing:
        m_transitionSecondsElapsed = 0.0;
        break;
    case FlightMode::WingBorne:
        m_flightPattern->beginFrom(m_position, m_attitude);
        break;
    case FlightMode::MotorsCut:
        m_fallSpeedMetersPerSecond = 0.0;
        break;
    case FlightMode::Disarmed:
    case FlightMode::Crashed:
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_attitude.levelOff();
        break;
    default:
        break;
    }

    return true;
}

SimulatedVehicle::FlightMode SimulatedVehicle::modeWantedByCommand(const QString &commandName,
                                                                   bool *wasUnderstood)
{
    *wasUnderstood = true;

    if (commandName == VehicleCommandName::Arm)           return FlightMode::ArmedOnGround;
    if (commandName == VehicleCommandName::Disarm)        return FlightMode::Disarmed;
    if (commandName == VehicleCommandName::Launch)        return FlightMode::HoverClimb;
    if (commandName == VehicleCommandName::ReturnToHome)  return FlightMode::ReturningHome;
    if (commandName == VehicleCommandName::Land)          return FlightMode::Landing;
    if (commandName == VehicleCommandName::EmergencyStop) return FlightMode::MotorsCut;

    *wasUnderstood = false;
    return FlightMode::Disarmed;
}

VehicleCommandAcknowledgment SimulatedVehicle::answerCommand(const VehicleCommandRequest &request)
{
    VehicleCommandAcknowledgment acknowledgment;
    acknowledgment.vehicleIdentifier = m_vehicleIdentifier;
    acknowledgment.requestIdentifier = request.requestIdentifier;
    acknowledgment.commandName = request.commandName;

    bool wasUnderstood = false;
    const FlightMode wantedMode = modeWantedByCommand(request.commandName, &wasUnderstood);

    if (!wasUnderstood) {
        acknowledgment.wasAccepted = false;
        acknowledgment.refusalReason = QStringLiteral("this vehicle does not know that order");
        return acknowledgment;
    }

    QString refusalReason;
    acknowledgment.wasAccepted = tryToChangeModeTo(wantedMode, &refusalReason);
    acknowledgment.refusalReason = refusalReason;
    return acknowledgment;
}

// ---------------------------------------------------------------------
// FLYING
// ---------------------------------------------------------------------
void SimulatedVehicle::advanceOneStep(double stepSeconds)
{
    if (m_flightMode != FlightMode::Disarmed && m_flightMode != FlightMode::Crashed)
        m_secondsFlown += stepSeconds;

    switch (m_flightMode) {
    case FlightMode::Disarmed:
    case FlightMode::Crashed:
    case FlightMode::ArmedOnGround:
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_attitude.setTargetRollDegrees(0.0);
        m_attitude.setTargetPitchDegrees(0.0);
        break;

    case FlightMode::HoverClimb:       flyHoverClimb(stepSeconds); break;
    case FlightMode::TransitionToWing: flyTransitionToWing(stepSeconds); break;
    case FlightMode::WingBorne:        flyWingBorne(stepSeconds); break;
    case FlightMode::ReturningHome:    flyReturnToHome(stepSeconds); break;
    case FlightMode::Landing:          flyLanding(stepSeconds); break;
    case FlightMode::MotorsCut:        fallWithMotorsCut(stepSeconds); break;
    }

    m_attitude.easeTowardTargets(stepSeconds, kAttitudeEaseRatePerSecond);
    drainBatteryFor(stepSeconds);
}

void SimulatedVehicle::flyHoverClimb(double stepSeconds)
{
    // Straight up, no forward speed. A hovering tail-sitter looks like
    // a helicopter to the numbers. It rocks a little, the way anything
    // holding a hover does.
    m_position.changeAltitudeBy(kClimbRateMetersPerSecond * stepSeconds);
    m_airspeedMetersPerSecond = 0.0;
    m_groundspeedMetersPerSecond = 0.0;
    m_attitude.setTargetRollDegrees(3.0 * qSin(m_secondsFlown * 0.9));
    m_attitude.setTargetPitchDegrees(2.0 * qSin(m_secondsFlown * 0.6));

    if (m_position.altitudeMetersAboveHome() >= kCruiseAltitudeMeters) {
        m_position.setAltitudeMetersAboveHome(kCruiseAltitudeMeters);
        QString ignoredReason;
        tryToChangeModeTo(FlightMode::TransitionToWing, &ignoredReason);
    }
}

void SimulatedVehicle::flyTransitionToWing(double stepSeconds)
{
    // Tipping over. Speed builds from nothing to cruise speed over a
    // few seconds, and the nose drops while it happens.
    m_transitionSecondsElapsed += stepSeconds;
    const double howFarAlong =
        qBound(0.0, m_transitionSecondsElapsed / kTransitionSeconds, 1.0);

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond * howFarAlong;
    m_groundspeedMetersPerSecond = m_airspeedMetersPerSecond;
    m_attitude.setTargetPitchDegrees(-11.0 * howFarAlong);
    m_attitude.setTargetRollDegrees(0.0);

    m_position.moveAlongHeading(m_attitude.headingDegrees(),
                                m_groundspeedMetersPerSecond * stepSeconds);

    if (m_transitionSecondsElapsed >= kTransitionSeconds) {
        QString ignoredReason;
        tryToChangeModeTo(FlightMode::WingBorne, &ignoredReason);
    }
}

void SimulatedVehicle::flyWingBorne(double stepSeconds)
{
    // The vehicle knows how to fly. The pattern knows where to go.
    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_position.setAltitudeMetersAboveHome(kCruiseAltitudeMeters);

    m_flightPattern->flyOneStep(stepSeconds, kCruiseSpeedMetersPerSecond,
                                m_position, m_attitude);
}

void SimulatedVehicle::flyReturnToHome(double stepSeconds)
{
    if (m_position.groundDistanceFromHomeMeters() < kCloseEnoughToHomeMeters) {
        QString ignoredReason;
        tryToChangeModeTo(FlightMode::Landing, &ignoredReason);
        return;
    }

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_position.setAltitudeMetersAboveHome(kCruiseAltitudeMeters);
    m_attitude.setTargetPitchDegrees(1.0);

    // Turn toward home instead of snapping the nose around. Work out
    // how far off the heading is, kept between minus and plus a
    // hundred eighty so the aircraft always turns the short way. Bank
    // into whichever side the turn is on, and stop banking once it is
    // lined up.
    double headingErrorDegrees =
        m_position.headingToHomeDegrees() - m_attitude.headingDegrees();
    while (headingErrorDegrees > 180.0)
        headingErrorDegrees -= 360.0;
    while (headingErrorDegrees < -180.0)
        headingErrorDegrees += 360.0;

    const double turnRateDegreesPerSecond =
        qRadiansToDegrees(kCruiseSpeedMetersPerSecond / kTurnRadiusMeters);
    const double mostWeCanTurnThisStep = turnRateDegreesPerSecond * stepSeconds;

    m_attitude.turnByDegrees(
        qBound(-mostWeCanTurnThisStep, headingErrorDegrees, mostWeCanTurnThisStep));

    if (qAbs(headingErrorDegrees) < 2.0)
        m_attitude.setTargetRollDegrees(0.0);
    else
        m_attitude.setTargetRollDegrees(headingErrorDegrees > 0.0 ? kReturnBankDegrees
                                                                  : -kReturnBankDegrees);

    m_position.moveAlongHeading(m_attitude.headingDegrees(),
                                kCruiseSpeedMetersPerSecond * stepSeconds);
}

void SimulatedVehicle::flyLanding(double stepSeconds)
{
    m_airspeedMetersPerSecond = 0.0;
    m_groundspeedMetersPerSecond = 0.0;
    m_attitude.setTargetRollDegrees(0.0);
    m_attitude.setTargetPitchDegrees(0.0);
    m_position.changeAltitudeBy(-kDescentRateMetersPerSecond * stepSeconds);

    if (m_position.altitudeMetersAboveHome() <= 0.0) {
        QString ignoredReason;
        tryToChangeModeTo(FlightMode::Disarmed, &ignoredReason);
    }
}

void SimulatedVehicle::fallWithMotorsCut(double stepSeconds)
{
    // No motors. No control. Gravity and whatever speed it had.
    //
    // Falling speed builds at gravity until the airframe is tumbling
    // hard enough to stop it building. Forward speed bleeds off with
    // nothing pushing it. The wings swing because nothing is holding
    // them anywhere.
    m_fallSpeedMetersPerSecond =
        qMin(kTerminalFallSpeedMetersPerSecond,
             m_fallSpeedMetersPerSecond + (kGravityMetersPerSecondSquared * stepSeconds));

    m_position.changeAltitudeBy(-m_fallSpeedMetersPerSecond * stepSeconds);

    m_groundspeedMetersPerSecond = qMax(0.0, m_groundspeedMetersPerSecond - (5.0 * stepSeconds));
    m_airspeedMetersPerSecond = m_groundspeedMetersPerSecond;
    m_position.moveAlongHeading(m_attitude.headingDegrees(),
                                m_groundspeedMetersPerSecond * stepSeconds);

    m_attitude.setTargetRollDegrees(55.0 * qSin(m_secondsFlown * 2.4));
    m_attitude.setTargetPitchDegrees(-38.0);

    if (m_position.altitudeMetersAboveHome() <= 0.0) {
        m_fallSpeedMetersPerSecond = 0.0;
        QString ignoredReason;
        tryToChangeModeTo(FlightMode::Crashed, &ignoredReason);
    }
}

void SimulatedVehicle::drainBatteryFor(double stepSeconds)
{
    // Holding the whole aircraft up on motors costs far more than
    // flying on the wing. That is the entire reason a tail-sitter tips
    // over. Nothing drains with the motors off.
    switch (m_flightMode) {
    case FlightMode::HoverClimb:
    case FlightMode::TransitionToWing:
    case FlightMode::Landing:
    case FlightMode::ArmedOnGround:
        m_battery.drainFor(stepSeconds, kHoveringDrainPercentPerSecond);
        break;

    case FlightMode::WingBorne:
    case FlightMode::ReturningHome:
        m_battery.drainFor(stepSeconds, kWingBorneDrainPercentPerSecond);
        break;

    case FlightMode::Disarmed:
    case FlightMode::Crashed:
    case FlightMode::MotorsCut:
        break;
    }
}

VehicleSitRep SimulatedVehicle::currentSitRep() const
{
    VehicleSitRep sitRep;

    sitRep.vehicleIdentifier = m_vehicleIdentifier;
    sitRep.flightModeName = flightModeName();
    sitRep.isArmed = isArmed();

    sitRep.gpsFixType = 3;

    // Satellite count wanders a little so the screen looks alive.
    sitRep.satelliteCount = 11 + static_cast<int>(qFloor(2.0 * qAbs(qSin(m_secondsFlown / 7.0))));

    // The radio gets weaker the farther away the aircraft is.
    sitRep.radioSignalPercent = static_cast<int>(
        qBound(0.0, 100.0 - (m_position.groundDistanceFromHomeMeters() / 12.0), 100.0));

    sitRep.position = m_position;
    sitRep.attitude = m_attitude;
    sitRep.battery = m_battery;

    sitRep.airspeedMetersPerSecond = m_airspeedMetersPerSecond;
    sitRep.groundspeedMetersPerSecond = m_groundspeedMetersPerSecond;

    return sitRep;
}
