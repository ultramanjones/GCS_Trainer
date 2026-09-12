#include "model/vehicleposition.h"

#include <QtMath>

namespace {

// The launch point used when nobody says otherwise. Verona,
// Pennsylvania. Any pair of real coordinates would do.
constexpr double kDefaultHomeLatitudeDegrees = 40.5062;
constexpr double kDefaultHomeLongitudeDegrees = -79.8556;

// One degree of latitude is about this many meters everywhere on
// earth. Longitude uses the same number shrunk by the cosine of the
// latitude, because the lines of longitude squeeze together as you go
// north.
//
// This is the flat earth shortcut. It is wrong over hundreds of miles
// and exact enough over a few thousand feet, which is all a practice
// vehicle ever flies.
constexpr double kMetersPerDegreeLatitude = 111320.0;

}  // namespace

VehiclePosition::VehiclePosition()
    : m_homeLatitudeDegrees(kDefaultHomeLatitudeDegrees)
    , m_homeLongitudeDegrees(kDefaultHomeLongitudeDegrees)
{
}

VehiclePosition::VehiclePosition(double homeLatitudeDegrees, double homeLongitudeDegrees)
    : m_homeLatitudeDegrees(homeLatitudeDegrees)
    , m_homeLongitudeDegrees(homeLongitudeDegrees)
{
}

double VehiclePosition::eastMetersFromHome() const { return m_eastMetersFromHome; }
double VehiclePosition::northMetersFromHome() const { return m_northMetersFromHome; }
double VehiclePosition::altitudeMetersAboveHome() const { return m_altitudeMetersAboveHome; }
double VehiclePosition::homeLatitudeDegrees() const { return m_homeLatitudeDegrees; }
double VehiclePosition::homeLongitudeDegrees() const { return m_homeLongitudeDegrees; }

double VehiclePosition::latitudeDegrees() const
{
    return m_homeLatitudeDegrees + (m_northMetersFromHome / kMetersPerDegreeLatitude);
}

double VehiclePosition::longitudeDegrees() const
{
    const double metersPerDegreeLongitude =
        kMetersPerDegreeLatitude * qCos(qDegreesToRadians(m_homeLatitudeDegrees));
    return m_homeLongitudeDegrees + (m_eastMetersFromHome / metersPerDegreeLongitude);
}

double VehiclePosition::groundDistanceFromHomeMeters() const
{
    return qSqrt((m_eastMetersFromHome * m_eastMetersFromHome)
               + (m_northMetersFromHome * m_northMetersFromHome));
}

double VehiclePosition::headingToHomeDegrees() const
{
    // A heading of zero means north. East uses the sine and north uses
    // the cosine, which is the opposite of the way angles are written
    // in a math class.
    double headingDegrees =
        qRadiansToDegrees(qAtan2(-m_eastMetersFromHome, -m_northMetersFromHome));

    while (headingDegrees < 0.0)
        headingDegrees += 360.0;
    while (headingDegrees >= 360.0)
        headingDegrees -= 360.0;

    return headingDegrees;
}

void VehiclePosition::moveAlongHeading(double headingDegrees, double distanceMeters)
{
    const double headingRadians = qDegreesToRadians(headingDegrees);
    m_eastMetersFromHome += qSin(headingRadians) * distanceMeters;
    m_northMetersFromHome += qCos(headingRadians) * distanceMeters;
}

void VehiclePosition::changeAltitudeBy(double metersUp)
{
    m_altitudeMetersAboveHome += metersUp;
    if (m_altitudeMetersAboveHome < 0.0)
        m_altitudeMetersAboveHome = 0.0;
}

void VehiclePosition::setAltitudeMetersAboveHome(double meters)
{
    m_altitudeMetersAboveHome = qMax(0.0, meters);
}
