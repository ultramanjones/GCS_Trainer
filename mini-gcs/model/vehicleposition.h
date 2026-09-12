#pragma once

#include <QtGlobal>

// Where a vehicle is.
//
// Position is held in meters east and north of the launch point,
// because that is what flying and drawing both want. Latitude and
// longitude are worked out from those meters on request. The launch
// point is carried inside this object, so a position can answer for
// itself without asking anything else where home is.
//
// This is a plain value. It copies, it has no Qt object in it, and it
// can be tested with no program running.
class VehiclePosition
{
public:
    VehiclePosition();
    VehiclePosition(double homeLatitudeDegrees, double homeLongitudeDegrees);

    double eastMetersFromHome() const;
    double northMetersFromHome() const;
    double altitudeMetersAboveHome() const;

    double homeLatitudeDegrees() const;
    double homeLongitudeDegrees() const;

    // Worked out from the meters and the launch point.
    double latitudeDegrees() const;
    double longitudeDegrees() const;

    // Straight line distance over the ground back to the launch point.
    double groundDistanceFromHomeMeters() const;

    // Which way to point to fly back to the launch point.
    double headingToHomeDegrees() const;

    void moveAlongHeading(double headingDegrees, double distanceMeters);
    void changeAltitudeBy(double metersUp);
    void setAltitudeMetersAboveHome(double meters);

private:
    double m_eastMetersFromHome = 0.0;
    double m_northMetersFromHome = 0.0;
    double m_altitudeMetersAboveHome = 0.0;

    double m_homeLatitudeDegrees = 0.0;
    double m_homeLongitudeDegrees = 0.0;
};
