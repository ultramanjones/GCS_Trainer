#include "viewmodel/vehiclestatusviewmodel.h"

#include <QDateTime>
#include <QtMath>

VehicleStatusViewModel::VehicleStatusViewModel(QObject *parent)
    : QObject(parent)
    , m_linkAgeTimer(this)
{
    m_linkAgeTimer.setInterval(1000);
    connect(&m_linkAgeTimer, &QTimer::timeout,
            this, &VehicleStatusViewModel::updateLinkAgeAndStaleFlag);
    m_linkAgeTimer.start();
}

QString VehicleStatusViewModel::flightModeName() const { return m_flightModeName; }
bool VehicleStatusViewModel::isArmed() const { return m_isArmed; }
int VehicleStatusViewModel::satelliteCount() const { return m_satelliteCount; }
int VehicleStatusViewModel::radioSignalPercent() const { return m_radioSignalPercent; }
int VehicleStatusViewModel::batteryPercent() const { return m_batteryPercent; }
double VehicleStatusViewModel::altitudeMetersAboveHome() const { return m_altitudeMetersAboveHome; }
double VehicleStatusViewModel::airspeedMetersPerSecond() const { return m_airspeedMetersPerSecond; }
double VehicleStatusViewModel::groundspeedMetersPerSecond() const { return m_groundspeedMetersPerSecond; }
double VehicleStatusViewModel::rollDegrees() const { return m_rollDegrees; }
double VehicleStatusViewModel::pitchDegrees() const { return m_pitchDegrees; }
double VehicleStatusViewModel::headingDegrees() const { return m_headingDegrees; }
int VehicleStatusViewModel::secondsSinceLastSnapshot() const { return m_secondsSinceLastSnapshot; }
bool VehicleStatusViewModel::isLinkStale() const { return m_isLinkStale; }

QString VehicleStatusViewModel::gpsFixDescription() const
{
    // Turning a number into words for the screen is viewmodel work.
    // It organizes data into the exact shape the view shows. The view
    // never has to learn what a 3 means.
    switch (m_gpsFixType) {
    case 3:  return QStringLiteral("3D Fix");
    case 2:  return QStringLiteral("2D Fix");
    default: return QStringLiteral("No Fix");
    }
}

void VehicleStatusViewModel::applyTelemetrySnapshot(TelemetrySnapshot snapshot)
{
    // Copy each field across, but only announce the ones that really
    // changed. Most fields sit still between two snapshots, so most
    // of these branches do nothing most of the time.

    if (m_flightModeName != snapshot.flightModeName) {
        m_flightModeName = snapshot.flightModeName;
        emit flightModeNameChanged();
    }
    if (m_isArmed != snapshot.isArmed) {
        m_isArmed = snapshot.isArmed;
        emit isArmedChanged();
    }
    if (m_gpsFixType != snapshot.gpsFixType) {
        m_gpsFixType = snapshot.gpsFixType;
        emit gpsFixDescriptionChanged();  // the words come from this number
    }
    if (m_satelliteCount != snapshot.satelliteCount) {
        m_satelliteCount = snapshot.satelliteCount;
        emit satelliteCountChanged();
    }
    if (m_radioSignalPercent != snapshot.radioSignalPercent) {
        m_radioSignalPercent = snapshot.radioSignalPercent;
        emit radioSignalPercentChanged();
    }
    if (m_batteryPercent != snapshot.batteryPercent) {
        m_batteryPercent = snapshot.batteryPercent;
        emit batteryPercentChanged();
    }

    // Numbers with a decimal point are compared with a small window
    // instead of an exact match. Two doubles almost never land on the
    // same value twice, so an exact check would fire the signal every
    // single time and defeat the whole point of checking.
    const double smallestChangeWorthTelling = 0.01;

    if (qAbs(m_altitudeMetersAboveHome - snapshot.altitudeMetersAboveHome) > smallestChangeWorthTelling) {
        m_altitudeMetersAboveHome = snapshot.altitudeMetersAboveHome;
        emit altitudeMetersAboveHomeChanged();
    }
    if (qAbs(m_airspeedMetersPerSecond - snapshot.airspeedMetersPerSecond) > smallestChangeWorthTelling) {
        m_airspeedMetersPerSecond = snapshot.airspeedMetersPerSecond;
        emit airspeedMetersPerSecondChanged();
    }
    if (qAbs(m_groundspeedMetersPerSecond - snapshot.groundspeedMetersPerSecond) > smallestChangeWorthTelling) {
        m_groundspeedMetersPerSecond = snapshot.groundspeedMetersPerSecond;
        emit groundspeedMetersPerSecondChanged();
    }
    if (qAbs(m_rollDegrees - snapshot.rollDegrees) > smallestChangeWorthTelling) {
        m_rollDegrees = snapshot.rollDegrees;
        emit rollDegreesChanged();
    }
    if (qAbs(m_pitchDegrees - snapshot.pitchDegrees) > smallestChangeWorthTelling) {
        m_pitchDegrees = snapshot.pitchDegrees;
        emit pitchDegreesChanged();
    }
    if (qAbs(m_headingDegrees - snapshot.headingDegrees) > smallestChangeWorthTelling) {
        m_headingDegrees = snapshot.headingDegrees;
        emit headingDegreesChanged();
    }

    // Write down that a snapshot just landed. The once-a-second timer
    // measures against this.
    m_lastSnapshotArrivedAtMilliseconds = QDateTime::currentMSecsSinceEpoch();

    if (m_isLinkStale) {
        m_isLinkStale = false;
        emit isLinkStaleChanged();
    }
}

void VehicleStatusViewModel::updateLinkAgeAndStaleFlag()
{
    if (m_lastSnapshotArrivedAtMilliseconds == 0)
        return;  // nothing has ever arrived, so there is nothing to measure

    const qint64 nowMilliseconds = QDateTime::currentMSecsSinceEpoch();
    const int ageSeconds =
        static_cast<int>((nowMilliseconds - m_lastSnapshotArrivedAtMilliseconds) / 1000);

    if (m_secondsSinceLastSnapshot != ageSeconds) {
        m_secondsSinceLastSnapshot = ageSeconds;
        emit secondsSinceLastSnapshotChanged();
    }

    const bool linkLooksStale = ageSeconds >= kLinkStaleThresholdSeconds;
    if (m_isLinkStale != linkLooksStale) {
        m_isLinkStale = linkLooksStale;
        emit isLinkStaleChanged();
    }
}
