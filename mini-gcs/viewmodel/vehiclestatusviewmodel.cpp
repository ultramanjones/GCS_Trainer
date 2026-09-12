#include "viewmodel/vehiclestatusviewmodel.h"

#include <QtMath>

#include "modelview/groundcontrolstation.h"
#include "modelview/mapvehicle.h"

VehicleStatusViewModel::VehicleStatusViewModel(GroundControlStation *groundControlStation,
                                               QObject *parent)
    : QObject(parent)
    , m_groundControlStation(groundControlStation)
    , m_reportAgeTimer(this)
{
    m_reportAgeTimer.setInterval(1000);
    connect(&m_reportAgeTimer, &QTimer::timeout,
            this, &VehicleStatusViewModel::refreshReportAge);
    m_reportAgeTimer.start();
}

QString VehicleStatusViewModel::flightModeName() const { return m_flightModeName; }
bool VehicleStatusViewModel::isArmed() const { return m_isArmed; }
int VehicleStatusViewModel::satelliteCount() const { return m_satelliteCount; }
int VehicleStatusViewModel::radioSignalPercent() const { return m_radioSignalPercent; }
int VehicleStatusViewModel::batteryPercent() const { return m_batteryPercent; }
double VehicleStatusViewModel::altitudeMetersAboveHome() const { return m_altitudeMetersAboveHome; }
double VehicleStatusViewModel::airspeedMetersPerSecond() const { return m_airspeedMetersPerSecond; }
double VehicleStatusViewModel::rollDegrees() const { return m_rollDegrees; }
double VehicleStatusViewModel::pitchDegrees() const { return m_pitchDegrees; }
double VehicleStatusViewModel::headingDegrees() const { return m_headingDegrees; }
int VehicleStatusViewModel::secondsSinceLastReport() const { return m_secondsSinceLastReport; }
bool VehicleStatusViewModel::isOutOfContact() const { return m_isOutOfContact; }

QString VehicleStatusViewModel::gpsFixDescription() const
{
    // Turning a number into words for the screen is organizing, which
    // is this layer's job. The view never has to learn what a 3 means.
    switch (m_gpsFixType) {
    case 3:  return QStringLiteral("3D Fix");
    case 2:  return QStringLiteral("2D Fix");
    default: return QStringLiteral("No Fix");
    }
}

bool VehicleStatusViewModel::hasMovedEnough(double oldValue, double newValue)
{
    return qAbs(oldValue - newValue) > kSmallestChangeWorthTelling;
}

void VehicleStatusViewModel::attachToActiveVehicle()
{
    if (!m_groundControlStation)
        return;

    MapVehicle *activeMapVehicle = m_groundControlStation->activeMapVehicle();
    if (activeMapVehicle == m_mapVehicle)
        return;

    m_mapVehicle = activeMapVehicle;
    refreshFromMapVehicle();
}

void VehicleStatusViewModel::refreshFromMapVehicle()
{
    if (!m_mapVehicle)
        return;

    if (m_flightModeName != m_mapVehicle->flightModeName()) {
        m_flightModeName = m_mapVehicle->flightModeName();
        emit flightModeNameChanged();
    }
    if (m_isArmed != m_mapVehicle->isArmed()) {
        m_isArmed = m_mapVehicle->isArmed();
        emit isArmedChanged();
    }
    if (m_gpsFixType != m_mapVehicle->gpsFixType()) {
        m_gpsFixType = m_mapVehicle->gpsFixType();
        emit gpsFixDescriptionChanged();  // the words come from this number
    }
    if (m_satelliteCount != m_mapVehicle->satelliteCount()) {
        m_satelliteCount = m_mapVehicle->satelliteCount();
        emit satelliteCountChanged();
    }
    if (m_radioSignalPercent != m_mapVehicle->radioSignalPercent()) {
        m_radioSignalPercent = m_mapVehicle->radioSignalPercent();
        emit radioSignalPercentChanged();
    }

    const int chargePercent =
        static_cast<int>(qRound(m_mapVehicle->lastConfirmedBattery().chargePercent()));
    if (m_batteryPercent != chargePercent) {
        m_batteryPercent = chargePercent;
        emit batteryPercentChanged();
    }

    const VehiclePosition &position = m_mapVehicle->lastConfirmedPosition();
    if (hasMovedEnough(m_altitudeMetersAboveHome, position.altitudeMetersAboveHome())) {
        m_altitudeMetersAboveHome = position.altitudeMetersAboveHome();
        emit altitudeMetersAboveHomeChanged();
    }

    if (hasMovedEnough(m_airspeedMetersPerSecond, m_mapVehicle->airspeedMetersPerSecond())) {
        m_airspeedMetersPerSecond = m_mapVehicle->airspeedMetersPerSecond();
        emit airspeedMetersPerSecondChanged();
    }

    const VehicleAttitude &attitude = m_mapVehicle->lastConfirmedAttitude();
    if (hasMovedEnough(m_rollDegrees, attitude.rollDegrees())) {
        m_rollDegrees = attitude.rollDegrees();
        emit rollDegreesChanged();
    }
    if (hasMovedEnough(m_pitchDegrees, attitude.pitchDegrees())) {
        m_pitchDegrees = attitude.pitchDegrees();
        emit pitchDegreesChanged();
    }
    if (hasMovedEnough(m_headingDegrees, attitude.headingDegrees())) {
        m_headingDegrees = attitude.headingDegrees();
        emit headingDegreesChanged();
    }

    if (m_isOutOfContact != m_mapVehicle->isOutOfContact()) {
        m_isOutOfContact = m_mapVehicle->isOutOfContact();
        emit isOutOfContactChanged();
    }

    refreshReportAge();
}

void VehicleStatusViewModel::refreshReportAge()
{
    if (!m_mapVehicle)
        return;

    const int ageSeconds = m_mapVehicle->secondsSinceLastReport();
    if (m_secondsSinceLastReport != ageSeconds) {
        m_secondsSinceLastReport = ageSeconds;
        emit secondsSinceLastReportChanged();
    }
}
