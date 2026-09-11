#pragma once

#include <QObject>
#include <QTimer>

#include "model/telemetrysnapshot.h"
#include "modelview/telemetrysource.h"

// VehicleModel holds the values the status bar shows. It never runs on
// the worker thread. It never receives data straight from
// TelemetrySource — main.cpp wires that connection with
// Qt::QueuedConnection, so by the time applySnapshot() runs, we are
// already back on the main thread and it is safe to touch QML-bound
// properties.
//
// Every setter checks the old value first. NOTIFY only fires when a
// value actually changed. That one rule is what keeps QML from
// repainting the whole bar 20 times a second for nothing.
class VehicleModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString flightMode READ flightMode NOTIFY flightModeChanged)
    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged)
    Q_PROPERTY(QString gpsFixLabel READ gpsFixLabel NOTIFY gpsFixLabelChanged)
    Q_PROPERTY(int satelliteCount READ satelliteCount NOTIFY satelliteCountChanged)
    Q_PROPERTY(int rssiPercent READ rssiPercent NOTIFY rssiPercentChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryPercentChanged)
    Q_PROPERTY(int linkAgeSeconds READ linkAgeSeconds NOTIFY linkAgeSecondsChanged)
    Q_PROPERTY(bool linkStale READ linkStale NOTIFY linkStaleChanged)

public:
    // telemetrySource is not owned here. main.cpp owns it and keeps it
    // alive on the worker thread; VehicleModel only keeps the pointer
    // so dropLink() can reach across to it.
    explicit VehicleModel(TelemetrySource *telemetrySource, QObject *parent = nullptr);

    QString flightMode() const;
    bool armed() const;
    QString gpsFixLabel() const;
    int satelliteCount() const;
    int rssiPercent() const;
    int batteryPercent() const;
    int linkAgeSeconds() const;
    bool linkStale() const;

public slots:
    // Connected to TelemetrySource::snapshotReady with
    // Qt::QueuedConnection. Runs on the main thread even though the
    // signal came from the worker thread.
    void applySnapshot(TelemetrySnapshot snapshot);

    // Called from the "Drop link" button in QML. Posts a call onto
    // the worker thread instead of touching telemetrySource directly
    // — telemetrySource belongs to that thread, not this one.
    void dropLink();

signals:
    void flightModeChanged();
    void armedChanged();
    void gpsFixLabelChanged();
    void satelliteCountChanged();
    void rssiPercentChanged();
    void batteryPercentChanged();
    void linkAgeSecondsChanged();
    void linkStaleChanged();

private slots:
    // Runs once a second off m_linkAgeTimer. Recomputes linkAgeSeconds
    // and linkStale from how long ago the last snapshot arrived.
    void tickLinkAge();

private:
    TelemetrySource *m_telemetrySource = nullptr;

    QString m_flightMode;
    bool m_armed = false;
    int m_gpsFixType = 0;
    int m_satelliteCount = 0;
    int m_rssiPercent = 0;
    int m_batteryPercent = 100;

    qint64 m_lastSnapshotAtMs = 0;
    int m_linkAgeSeconds = 0;
    bool m_linkStale = false;
    QTimer m_linkAgeTimer;

    static constexpr int kLinkStaleThresholdSeconds = 5;
};
