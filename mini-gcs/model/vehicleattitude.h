#pragma once

// How a vehicle is sitting in the air.
//
// Roll and pitch are held two ways: where they are now, and where they
// are being asked to go. Nothing snaps. Every step moves the real
// value part of the way toward the target, which is what makes an
// instrument look like an aircraft instead of a slide show.
//
// Heading is different and is not eased. A heading changes because the
// aircraft turned, and the turn itself is already gradual.
//
// This is a plain value. It copies and has no Qt object in it.
class VehicleAttitude
{
public:
    VehicleAttitude();

    double rollDegrees() const;
    double pitchDegrees() const;
    double headingDegrees() const;

    double targetRollDegrees() const;
    double targetPitchDegrees() const;

    void setTargetRollDegrees(double degrees);
    void setTargetPitchDegrees(double degrees);

    // Point the nose somewhere, right now.
    void setHeadingDegrees(double degrees);

    // Turn by this much. Positive is to the right. The result is kept
    // between zero and three hundred sixty.
    void turnByDegrees(double degrees);

    // Move roll and pitch part of the way toward their targets.
    void easeTowardTargets(double stepSeconds, double easeRatePerSecond);

    // Put the wings level and the nose on the horizon, immediately.
    void levelOff();

private:
    double m_rollDegrees = 0.0;
    double m_pitchDegrees = 0.0;
    double m_headingDegrees = 0.0;

    double m_targetRollDegrees = 0.0;
    double m_targetPitchDegrees = 0.0;
};
