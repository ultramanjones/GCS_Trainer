#include "viewmodel/vehiclemodel.h"

#include <QDateTime>
#include <QMetaObject>

VehicleModel::VehicleModel(TelemetrySource *telemetrySource, QObject *parent)
    : QObject(parent)
    , m_telemetrySource(telemetrySource)
{
    m_linkAgeTimer.setInterval(1000);
    connect(&m_linkAgeTimer, &QTimer::timeout, this, &VehicleModel::tickLinkAge);
    m_linkAgeTimer.start();
}

QString VehicleModel::flightMode() const { return m_flightMode; }
bool VehicleModel::armed() const { return m_armed; }
int VehicleModel::satelliteCount() const { return m_satelliteCount; }
int VehicleModel::rssiPercent() const { return m_rssiPercent; }
int VehicleModel::batteryPercent() const { return m_batteryPercent; }
int VehicleModel::linkAgeSeconds() const { return m_linkAgeSeconds; }
bool VehicleModel::linkStale() const { return m_linkStale; }

QString VehicleModel::gpsFixLabel() const
{
    // Translating a numeric code into words for the screen is
    // ViewModel work: organizing data into exactly what the view
    // should display. The view itself never has to know what a "3"
    // means.
    switch (m_gpsFixType) {
    case 3:  return QStringLiteral("3D Fix");
    case 2:  return QStringLiteral("2D Fix");
    default: return QStringLiteral("No Fix");
    }
}

void VehicleModel::applySnapshot(TelemetrySnapshot snapshot)
{
    // Copy each field across, but only emit the matching *Changed
    // signal when the value actually moved. This is the rule that
    // keeps QML from repainting the whole bar 20 times a second for
    // nothing — most fields barely change between two snapshots, so
    // most of these branches don't fire most of the time.
    if (m_flightMode != snapshot.flightMode) {
        m_flightMode = snapshot.flightMode;
        emit flightModeChanged();
    }
    if (m_armed != snapshot.armed) {
        m_armed = snapshot.armed;
        emit armedChanged();
    }
    if (m_gpsFixType != snapshot.gpsFixType) {
        m_gpsFixType = snapshot.gpsFixType;
        emit gpsFixLabelChanged();  // the label is derived from this field
    }
    if (m_satelliteCount != snapshot.satelliteCount) {
        m_satelliteCount = snapshot.satelliteCount;
        emit satelliteCountChanged();
    }
    if (m_rssiPercent != snapshot.rssiPercent) {
        m_rssiPercent = snapshot.rssiPercent;
        emit rssiPercentChanged();
    }
    if (m_batteryPercent != snapshot.batteryPercent) {
        m_batteryPercent = snapshot.batteryPercent;
        emit batteryPercentChanged();
    }

    // Record that a snapshot just arrived. tickLinkAge() uses this to
    // compute how long it has been since the last one.
    m_lastSnapshotAtMs = QDateTime::currentMSecsSinceEpoch();
}

void VehicleModel::dropLink()
{
    // telemetrySource lives on the worker thread. Calling
    // stopLink() directly from here would run worker-thread code on
    // the main thread — unsafe in general, even though this
    // particular slot only touches two QTimers. invokeMethod with a
    // queued connection posts the call to run on telemetrySource's
    // own thread instead, the same way a real signal would.
    QMetaObject::invokeMethod(m_telemetrySource, "stopLink", Qt::QueuedConnection);
}

void VehicleModel::tickLinkAge()
{
    if (m_lastSnapshotAtMs == 0)
        return;  // no snapshot has arrived yet

    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    const int ageSeconds = static_cast<int>((nowMs - m_lastSnapshotAtMs) / 1000);

    if (m_linkAgeSeconds != ageSeconds) {
        m_linkAgeSeconds = ageSeconds;
        emit linkAgeSecondsChanged();
    }

    const bool stale = ageSeconds >= kLinkStaleThresholdSeconds;
    if (m_linkStale != stale) {
        m_linkStale = stale;
        emit linkStaleChanged();
    }
}
