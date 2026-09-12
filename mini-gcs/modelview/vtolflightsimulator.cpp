#include "modelview/vtolflightsimulator.h"

#include <QtMath>

VtolFlightSimulator::VtolFlightSimulator()
{
}

QString VtolFlightSimulator::flightStageDisplayName() const
{
    switch (m_flightStage) {
    case FlightStage::Disarmed:         return QStringLiteral("Disarmed");
    case FlightStage::ArmedOnGround:    return QStringLiteral("Armed");
    case FlightStage::HoverClimb:       return QStringLiteral("Hover");
    case FlightStage::TransitionToWing: return QStringLiteral("Transition");
    case FlightStage::WingBorneCircuit: return QStringLiteral("Cruise");
    case FlightStage::ReturningHome:    return QStringLiteral("Return");
    case FlightStage::Landing:          return QStringLiteral("Land");
    }
    return QStringLiteral("Unknown");
}

bool VtolFlightSimulator::tryToApplyCommand(const QString &commandName)
{
    if (commandName == QLatin1String("Arm")) {
        if (m_flightStage != FlightStage::Disarmed)
            return false;
        m_flightStage = FlightStage::ArmedOnGround;
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

void VtolFlightSimulator::advanceOneStep(double stepSeconds)
{
    if (m_flightStage != FlightStage::Disarmed)
        m_secondsFlown += stepSeconds;

    switch (m_flightStage) {

    case FlightStage::Disarmed:
    case FlightStage::ArmedOnGround:
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_rollDegrees = 0.0;
        m_pitchDegrees = 0.0;
        break;

    case FlightStage::HoverClimb:
        // Straight up, nose level, no forward speed. A tail-sitter
        // hovering looks like a helicopter to the numbers.
        m_altitudeMetersAboveHome += kClimbRateMetersPerSecond * stepSeconds;
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_rollDegrees = 0.0;
        m_pitchDegrees = 0.0;
        if (m_altitudeMetersAboveHome >= kCruiseAltitudeMeters) {
            m_altitudeMetersAboveHome = kCruiseAltitudeMeters;
            m_flightStage = FlightStage::TransitionToWing;
            m_transitionSecondsElapsed = 0.0;
        }
        break;

    case FlightStage::TransitionToWing: {
        // Tipping over. Speed builds up from nothing to cruise speed
        // over a few seconds, and the nose drops while it happens.
        m_transitionSecondsElapsed += stepSeconds;
        const double howFarAlong =
            qBound(0.0, m_transitionSecondsElapsed / kTransitionSeconds, 1.0);
        m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond * howFarAlong;
        m_groundspeedMetersPerSecond = m_airspeedMetersPerSecond;
        m_pitchDegrees = -10.0 * howFarAlong;
        m_rollDegrees = 0.0;

        // Creep forward along the current heading while tipping over.
        const double headingRadians = qDegreesToRadians(m_headingDegrees);
        m_eastMetersFromHome += qSin(headingRadians) * m_groundspeedMetersPerSecond * stepSeconds;
        m_northMetersFromHome += qCos(headingRadians) * m_groundspeedMetersPerSecond * stepSeconds;

        if (m_transitionSecondsElapsed >= kTransitionSeconds) {
            m_flightStage = FlightStage::WingBorneCircuit;

            // Work out where we are on the circle from where we are
            // standing, so the switch to the circuit does not jump.
            m_circuitAngleRadians = qAtan2(m_eastMetersFromHome,
                                           m_northMetersFromHome - kCircuitRadiusMeters);
        }
        break;
    }

    case FlightStage::WingBorneCircuit:
        advanceWingBorneCircuit(stepSeconds);
        break;

    case FlightStage::ReturningHome:
        advanceReturnToHome(stepSeconds);
        break;

    case FlightStage::Landing:
        // Straight down, wherever we are.
        m_airspeedMetersPerSecond = 0.0;
        m_groundspeedMetersPerSecond = 0.0;
        m_rollDegrees = 0.0;
        m_pitchDegrees = 0.0;
        m_altitudeMetersAboveHome -= kDescentRateMetersPerSecond * stepSeconds;
        if (m_altitudeMetersAboveHome <= 0.0) {
            m_altitudeMetersAboveHome = 0.0;
            m_flightStage = FlightStage::Disarmed;
        }
        break;
    }

    // Battery drains faster when the motors are working harder.
    if (m_flightStage == FlightStage::HoverClimb
        || m_flightStage == FlightStage::TransitionToWing
        || m_flightStage == FlightStage::Landing) {
        m_batteryPercent -= 0.06 * stepSeconds;
    } else if (m_flightStage != FlightStage::Disarmed) {
        m_batteryPercent -= 0.02 * stepSeconds;
    }
    m_batteryPercent = qBound(0.0, m_batteryPercent, 100.0);
}

void VtolFlightSimulator::advanceWingBorneCircuit(double stepSeconds)
{
    // Fly a big circle. The circle sits one radius north of the
    // launch point, so the aircraft starts the circuit right where it
    // took off instead of jumping across the map.
    //
    // The math, spelled out:
    //   How fast we go around a circle is speed divided by radius.
    //   That gives radians per second. Multiply by the time step and
    //   you get how far around we moved this tick.
    //   Position on the circle is then the center plus radius times
    //   the sine and cosine of the angle.
    //   Heading is the direction of travel, which on a circle is the
    //   angle plus ninety degrees.
    const double radiansPerSecond = kCruiseSpeedMetersPerSecond / kCircuitRadiusMeters;
    m_circuitAngleRadians += radiansPerSecond * stepSeconds;

    m_eastMetersFromHome = kCircuitRadiusMeters * qSin(m_circuitAngleRadians);
    m_northMetersFromHome = kCircuitRadiusMeters
                          + kCircuitRadiusMeters * qCos(m_circuitAngleRadians);

    m_headingDegrees = qRadiansToDegrees(m_circuitAngleRadians) + 90.0;
    while (m_headingDegrees < 0.0)
        m_headingDegrees += 360.0;
    while (m_headingDegrees >= 360.0)
        m_headingDegrees -= 360.0;

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;

    // Banked over into the turn, nose a touch high.
    m_rollDegrees = 18.0;
    m_pitchDegrees = 2.0;

    m_altitudeMetersAboveHome = kCruiseAltitudeMeters;
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

    m_headingDegrees = qRadiansToDegrees(qAtan2(eastToGo, northToGo));
    while (m_headingDegrees < 0.0)
        m_headingDegrees += 360.0;

    m_airspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_groundspeedMetersPerSecond = kCruiseSpeedMetersPerSecond;
    m_rollDegrees = 6.0;
    m_pitchDegrees = 1.0;

    const double stepMeters = kCruiseSpeedMetersPerSecond * stepSeconds;
    m_eastMetersFromHome += (eastToGo / distanceMeters) * stepMeters;
    m_northMetersFromHome += (northToGo / distanceMeters) * stepMeters;
}

TelemetrySnapshot VtolFlightSimulator::currentTelemetrySnapshot() const
{
    TelemetrySnapshot snapshot;

    snapshot.flightModeName = flightStageDisplayName();
    snapshot.isArmed = m_flightStage != FlightStage::Disarmed;

    snapshot.gpsFixType = 3;

    // Satellite count wanders a little so the screen looks alive.
    snapshot.satelliteCount = 11 + static_cast<int>(qFloor(2.0 * qAbs(qSin(m_secondsFlown / 7.0))));

    // Radio gets weaker the farther away the aircraft is. Twelve
    // hundred meters out is where it reaches zero.
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
    // the latitude, because the lines of longitude squeeze together
    // as you go north. This is the flat-earth shortcut. It is wrong
    // over hundreds of miles and exact enough over a few thousand
    // feet, which is all this practice vehicle ever flies.
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
