#include "viewmodel/vehiclegroundtrackviewmodel.h"

#include "modelview/groundcontrolstation.h"
#include "modelview/mapvehicle.h"

VehicleGroundTrackViewModel::VehicleGroundTrackViewModel(GroundControlStation *groundControlStation,
                                                         QObject *parent)
    : QObject(parent)
    , m_groundControlStation(groundControlStation)
{
}

double VehicleGroundTrackViewModel::vehicleEastMetersFromHome() const { return m_vehicleEastMetersFromHome; }
double VehicleGroundTrackViewModel::vehicleNorthMetersFromHome() const { return m_vehicleNorthMetersFromHome; }
double VehicleGroundTrackViewModel::vehicleHeadingDegrees() const { return m_vehicleHeadingDegrees; }
double VehicleGroundTrackViewModel::vehicleLatitudeDegrees() const { return m_vehicleLatitudeDegrees; }
double VehicleGroundTrackViewModel::vehicleLongitudeDegrees() const { return m_vehicleLongitudeDegrees; }
bool VehicleGroundTrackViewModel::isOutOfContact() const { return m_isOutOfContact; }
int VehicleGroundTrackViewModel::mapRevisionNumber() const { return m_mapRevisionNumber; }

QVariantList VehicleGroundTrackViewModel::groundTrackPoints() const
{
    QVariantList pointsForDrawing;
    if (!m_mapVehicle)
        return pointsForDrawing;

    const QList<QPointF> trackPoints = m_mapVehicle->groundTrackPoints();
    pointsForDrawing.reserve(trackPoints.size());
    for (const QPointF &onePoint : trackPoints)
        pointsForDrawing.append(QVariant::fromValue(onePoint));
    return pointsForDrawing;
}

void VehicleGroundTrackViewModel::clearGroundTrack()
{
    if (m_mapVehicle)
        m_mapVehicle->clearGroundTrack();
}

void VehicleGroundTrackViewModel::attachToActiveVehicle()
{
    if (!m_groundControlStation)
        return;

    MapVehicle *activeMapVehicle = m_groundControlStation->activeMapVehicle();
    if (activeMapVehicle == m_mapVehicle)
        return;

    m_mapVehicle = activeMapVehicle;
    refreshFromMapVehicle();
}

void VehicleGroundTrackViewModel::refreshFromMapVehicle()
{
    if (!m_mapVehicle)
        return;

    const VehiclePosition &position = m_mapVehicle->lastConfirmedPosition();
    const VehicleAttitude &attitude = m_mapVehicle->lastConfirmedAttitude();

    m_vehicleEastMetersFromHome = position.eastMetersFromHome();
    m_vehicleNorthMetersFromHome = position.northMetersFromHome();
    m_vehicleHeadingDegrees = attitude.headingDegrees();
    m_vehicleLatitudeDegrees = position.latitudeDegrees();
    m_vehicleLongitudeDegrees = position.longitudeDegrees();

    emit vehiclePositionChanged();

    if (m_isOutOfContact != m_mapVehicle->isOutOfContact()) {
        m_isOutOfContact = m_mapVehicle->isOutOfContact();
        emit isOutOfContactChanged();
    }

    ++m_mapRevisionNumber;
    emit mapRevisionNumberChanged();
}
