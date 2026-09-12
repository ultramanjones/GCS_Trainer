#include "model/vehicleattitude.h"

#include <QtMath>

namespace {

double wrappedDegrees(double degrees)
{
    while (degrees < 0.0)
        degrees += 360.0;
    while (degrees >= 360.0)
        degrees -= 360.0;
    return degrees;
}

}  // namespace

VehicleAttitude::VehicleAttitude()
{
}

double VehicleAttitude::rollDegrees() const { return m_rollDegrees; }
double VehicleAttitude::pitchDegrees() const { return m_pitchDegrees; }
double VehicleAttitude::headingDegrees() const { return m_headingDegrees; }
double VehicleAttitude::targetRollDegrees() const { return m_targetRollDegrees; }
double VehicleAttitude::targetPitchDegrees() const { return m_targetPitchDegrees; }

void VehicleAttitude::setTargetRollDegrees(double degrees) { m_targetRollDegrees = degrees; }
void VehicleAttitude::setTargetPitchDegrees(double degrees) { m_targetPitchDegrees = degrees; }

void VehicleAttitude::setHeadingDegrees(double degrees)
{
    m_headingDegrees = wrappedDegrees(degrees);
}

void VehicleAttitude::turnByDegrees(double degrees)
{
    m_headingDegrees = wrappedDegrees(m_headingDegrees + degrees);
}

void VehicleAttitude::easeTowardTargets(double stepSeconds, double easeRatePerSecond)
{
    // Move part of the way toward the target every step instead of
    // snapping there.
    //
    // The fraction moved each step is the ease rate times the length of
    // the step. It is held at one so a long step can never overshoot
    // and start swinging back and forth. This is the same one line of
    // smoothing every flight instrument uses.
    const double fractionOfTheWay = qMin(1.0, easeRatePerSecond * stepSeconds);

    m_rollDegrees += (m_targetRollDegrees - m_rollDegrees) * fractionOfTheWay;
    m_pitchDegrees += (m_targetPitchDegrees - m_pitchDegrees) * fractionOfTheWay;
}

void VehicleAttitude::levelOff()
{
    m_rollDegrees = 0.0;
    m_pitchDegrees = 0.0;
    m_targetRollDegrees = 0.0;
    m_targetPitchDegrees = 0.0;
}
