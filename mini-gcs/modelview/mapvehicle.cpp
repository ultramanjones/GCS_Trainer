#include "modelview/mapvehicle.h"

#include <QDateTime>
#include <QtMath>

MapVehicle::MapVehicle(int vehicleIdentifier, QObject *parent)
    : QObject(parent)
    , m_vehicleIdentifier(vehicleIdentifier)
{
}

int MapVehicle::vehicleIdentifier() const { return m_vehicleIdentifier; }
QString MapVehicle::flightModeName() const { return m_flightModeName; }
bool MapVehicle::isArmed() const { return m_isArmed; }
int MapVehicle::gpsFixType() const { return m_gpsFixType; }
int MapVehicle::satelliteCount() const { return m_satelliteCount; }
int MapVehicle::radioSignalPercent() const { return m_radioSignalPercent; }
double MapVehicle::airspeedMetersPerSecond() const { return m_airspeedMetersPerSecond; }
double MapVehicle::groundspeedMetersPerSecond() const { return m_groundspeedMetersPerSecond; }

const VehiclePosition &MapVehicle::lastConfirmedPosition() const { return m_lastConfirmedPosition; }
const VehicleAttitude &MapVehicle::lastConfirmedAttitude() const { return m_lastConfirmedAttitude; }
const VehicleBattery &MapVehicle::lastConfirmedBattery() const { return m_lastConfirmedBattery; }

bool MapVehicle::isOutOfContact() const { return m_isOutOfContact; }

int MapVehicle::secondsSinceLastReport() const
{
    if (m_lastReportArrivedAtMilliseconds == 0)
        return 0;  // nothing has ever arrived, so there is nothing to measure

    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();
    return static_cast<int>((nowMilliseconds - m_lastReportArrivedAtMilliseconds) / 1000);
}

QList<QPointF> MapVehicle::groundTrackPoints() const
{
    return m_groundTrackPoints;
}

void MapVehicle::applySitRep(VehicleSitRep sitRep)
{
    const QString previousFlightModeName = m_flightModeName;

    m_flightModeName = sitRep.flightModeName;
    m_isArmed = sitRep.isArmed;
    m_gpsFixType = sitRep.gpsFixType;
    m_satelliteCount = sitRep.satelliteCount;
    m_radioSignalPercent = sitRep.radioSignalPercent;
    m_airspeedMetersPerSecond = sitRep.airspeedMetersPerSecond;
    m_groundspeedMetersPerSecond = sitRep.groundspeedMetersPerSecond;

    m_lastConfirmedPosition = sitRep.position;
    m_lastConfirmedAttitude = sitRep.attitude;
    m_lastConfirmedBattery = sitRep.battery;

    m_lastReportArrivedAtMilliseconds = QDateTime::currentMSecsSinceEpoch();

    // Keep the track of where this vehicle has been.
    const QPointF newestPoint(m_lastConfirmedPosition.eastMetersFromHome(),
                              m_lastConfirmedPosition.northMetersFromHome());

    bool trackGrew = false;
    if (m_groundTrackPoints.isEmpty()) {
        m_groundTrackPoints.append(newestPoint);
        trackGrew = true;
    } else {
        const QPointF lastPoint = m_groundTrackPoints.last();
        const double eastGap = newestPoint.x() - lastPoint.x();
        const double northGap = newestPoint.y() - lastPoint.y();
        const double metersMoved = qSqrt((eastGap * eastGap) + (northGap * northGap));
        if (metersMoved >= kMinimumMetersBetweenTrackPoints) {
            m_groundTrackPoints.append(newestPoint);
            trackGrew = true;
        }
    }

    if (trackGrew) {
        if (m_groundTrackPoints.size() > kMaximumTrackPoints)
            m_groundTrackPoints.removeFirst();
        emit groundTrackChanged();
    }

    if (m_flightModeName != previousFlightModeName)
        emit flightModeChanged(m_flightModeName);

    emit sitRepApplied();
}

void MapVehicle::markOutOfContact()
{
    if (m_isOutOfContact)
        return;
    m_isOutOfContact = true;
    emit contactStateChanged();
}

void MapVehicle::markBackInContact()
{
    if (!m_isOutOfContact)
        return;
    m_isOutOfContact = false;
    emit contactStateChanged();
}

void MapVehicle::clearGroundTrack()
{
    m_groundTrackPoints.clear();
    emit groundTrackChanged();
}
