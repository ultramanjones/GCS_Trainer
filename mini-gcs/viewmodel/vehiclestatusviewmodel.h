#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

#include "model/telemetrysnapshot.h"

// Everything the status bar and the flight instrument put on screen.
//
// QGroundControl calls its version of this object Vehicle. It is the
// one object the whole screen reads from.
//
// This object never runs on the worker thread. It never hears from
// the radio link directly either. The composition root wires that
// connection with a queued connection, so by the time
// applyTelemetrySnapshot runs we are already back on the thread that
// draws the window, and touching screen-bound values is safe.
//
// Every value is checked before it is stored. The NOTIFY signal only
// fires when a value really moved. That one rule is what keeps QML
// from redrawing the whole bar twenty times a second for nothing.
class VehicleStatusViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString flightModeName READ flightModeName NOTIFY flightModeNameChanged)
    Q_PROPERTY(bool isArmed READ isArmed NOTIFY isArmedChanged)
    Q_PROPERTY(QString gpsFixDescription READ gpsFixDescription NOTIFY gpsFixDescriptionChanged)
    Q_PROPERTY(int satelliteCount READ satelliteCount NOTIFY satelliteCountChanged)
    Q_PROPERTY(int radioSignalPercent READ radioSignalPercent NOTIFY radioSignalPercentChanged)
    Q_PROPERTY(int batteryPercent READ batteryPercent NOTIFY batteryPercentChanged)

    Q_PROPERTY(double altitudeMetersAboveHome READ altitudeMetersAboveHome NOTIFY altitudeMetersAboveHomeChanged)
    Q_PROPERTY(double airspeedMetersPerSecond READ airspeedMetersPerSecond NOTIFY airspeedMetersPerSecondChanged)
    Q_PROPERTY(double groundspeedMetersPerSecond READ groundspeedMetersPerSecond NOTIFY groundspeedMetersPerSecondChanged)

    Q_PROPERTY(double rollDegrees READ rollDegrees NOTIFY rollDegreesChanged)
    Q_PROPERTY(double pitchDegrees READ pitchDegrees NOTIFY pitchDegreesChanged)
    Q_PROPERTY(double headingDegrees READ headingDegrees NOTIFY headingDegreesChanged)

    Q_PROPERTY(int secondsSinceLastSnapshot READ secondsSinceLastSnapshot NOTIFY secondsSinceLastSnapshotChanged)
    Q_PROPERTY(bool isLinkStale READ isLinkStale NOTIFY isLinkStaleChanged)

public:
    explicit VehicleStatusViewModel(QObject *parent = nullptr);

    QString flightModeName() const;
    bool isArmed() const;
    QString gpsFixDescription() const;
    int satelliteCount() const;
    int radioSignalPercent() const;
    int batteryPercent() const;

    double altitudeMetersAboveHome() const;
    double airspeedMetersPerSecond() const;
    double groundspeedMetersPerSecond() const;

    double rollDegrees() const;
    double pitchDegrees() const;
    double headingDegrees() const;

    int secondsSinceLastSnapshot() const;
    bool isLinkStale() const;

public slots:
    // Connected to the radio link with a queued connection in the
    // composition root. Runs on the main thread even though the
    // snapshot was built on the worker thread.
    void applyTelemetrySnapshot(TelemetrySnapshot snapshot);

signals:
    void flightModeNameChanged();
    void isArmedChanged();
    void gpsFixDescriptionChanged();
    void satelliteCountChanged();
    void radioSignalPercentChanged();
    void batteryPercentChanged();
    void altitudeMetersAboveHomeChanged();
    void airspeedMetersPerSecondChanged();
    void groundspeedMetersPerSecondChanged();
    void rollDegreesChanged();
    void pitchDegreesChanged();
    void headingDegreesChanged();
    void secondsSinceLastSnapshotChanged();
    void isLinkStaleChanged();

private slots:
    // Runs once a second. Works out how long it has been since a
    // snapshot arrived, and whether that is long enough to call the
    // link stale.
    void updateLinkAgeAndStaleFlag();

private:
    QString m_flightModeName;
    bool m_isArmed = false;
    int m_gpsFixType = 0;
    int m_satelliteCount = 0;
    int m_radioSignalPercent = 0;
    int m_batteryPercent = 100;

    double m_altitudeMetersAboveHome = 0.0;
    double m_airspeedMetersPerSecond = 0.0;
    double m_groundspeedMetersPerSecond = 0.0;

    double m_rollDegrees = 0.0;
    double m_pitchDegrees = 0.0;
    double m_headingDegrees = 0.0;

    qint64 m_lastSnapshotArrivedAtMilliseconds = 0;
    int m_secondsSinceLastSnapshot = 0;
    bool m_isLinkStale = false;
    QTimer m_linkAgeTimer;

    static constexpr int kLinkStaleThresholdSeconds = 3;
};
