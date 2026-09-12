#include "viewmodel/flightmapviewmodel.h"

#include <QtMath>

FlightMapViewModel::FlightMapViewModel(QObject *parent)
    : QObject(parent)
{
}

double FlightMapViewModel::vehicleEastMetersFromHome() const { return m_vehicleEastMetersFromHome; }
double FlightMapViewModel::vehicleNorthMetersFromHome() const { return m_vehicleNorthMetersFromHome; }
double FlightMapViewModel::vehicleHeadingDegrees() const { return m_vehicleHeadingDegrees; }
double FlightMapViewModel::vehicleLatitudeDegrees() const { return m_vehicleLatitudeDegrees; }
double FlightMapViewModel::vehicleLongitudeDegrees() const { return m_vehicleLongitudeDegrees; }
int FlightMapViewModel::mapRevisionNumber() const { return m_mapRevisionNumber; }

QVariantList FlightMapViewModel::breadcrumbTrailPoints() const
{
    QVariantList pointsForDrawing;
    pointsForDrawing.reserve(m_breadcrumbTrailPoints.size());
    for (const QPointF &onePoint : m_breadcrumbTrailPoints)
        pointsForDrawing.append(QVariant::fromValue(onePoint));
    return pointsForDrawing;
}

void FlightMapViewModel::clearBreadcrumbTrail()
{
    m_breadcrumbTrailPoints.clear();
    ++m_mapRevisionNumber;
    emit mapRevisionNumberChanged();
}

void FlightMapViewModel::applyTelemetrySnapshot(TelemetrySnapshot snapshot)
{
    m_vehicleEastMetersFromHome = snapshot.eastMetersFromHome;
    m_vehicleNorthMetersFromHome = snapshot.northMetersFromHome;
    m_vehicleHeadingDegrees = snapshot.headingDegrees;
    m_vehicleLatitudeDegrees = snapshot.latitudeDegrees;
    m_vehicleLongitudeDegrees = snapshot.longitudeDegrees;

    const QPointF newestPosition(snapshot.eastMetersFromHome, snapshot.northMetersFromHome);

    bool trailGrew = false;
    if (m_breadcrumbTrailPoints.isEmpty()) {
        m_breadcrumbTrailPoints.append(newestPosition);
        trailGrew = true;
    } else {
        const QPointF lastCrumb = m_breadcrumbTrailPoints.last();
        const double eastGap = newestPosition.x() - lastCrumb.x();
        const double northGap = newestPosition.y() - lastCrumb.y();
        const double metersMoved = qSqrt(eastGap * eastGap + northGap * northGap);
        if (metersMoved >= kMinimumMetersBetweenCrumbs) {
            m_breadcrumbTrailPoints.append(newestPosition);
            trailGrew = true;
        }
    }

    if (trailGrew && m_breadcrumbTrailPoints.size() > kMaximumTrailPoints)
        m_breadcrumbTrailPoints.removeFirst();

    emit vehiclePositionChanged();

    ++m_mapRevisionNumber;
    emit mapRevisionNumberChanged();
}
