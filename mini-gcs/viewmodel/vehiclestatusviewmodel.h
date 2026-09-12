#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

class GroundControlStation;
class MapVehicle;

// Everything the status bar and the flight instrument put on screen.
//
// It holds no truth of its own. The truth is the map vehicle, one
// layer down. This object organizes that truth into the exact shape
// the screen wants, and decides what is worth telling QML about.
//
// That last part is the whole job. A new report arrives twenty times a
// second, and most values in it did not move. Every property here is
// checked before it is announced, so QML only re-draws what changed.
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

    Q_PROPERTY(double rollDegrees READ rollDegrees NOTIFY rollDegreesChanged)
    Q_PROPERTY(double pitchDegrees READ pitchDegrees NOTIFY pitchDegreesChanged)
    Q_PROPERTY(double headingDegrees READ headingDegrees NOTIFY headingDegreesChanged)

    Q_PROPERTY(int secondsSinceLastReport READ secondsSinceLastReport NOTIFY secondsSinceLastReportChanged)
    Q_PROPERTY(bool isOutOfContact READ isOutOfContact NOTIFY isOutOfContactChanged)

public:
    explicit VehicleStatusViewModel(GroundControlStation *groundControlStation,
                                    QObject *parent = nullptr);

    QString flightModeName() const;
    bool isArmed() const;
    QString gpsFixDescription() const;
    int satelliteCount() const;
    int radioSignalPercent() const;
    int batteryPercent() const;
    double altitudeMetersAboveHome() const;
    double airspeedMetersPerSecond() const;
    double rollDegrees() const;
    double pitchDegrees() const;
    double headingDegrees() const;
    int secondsSinceLastReport() const;
    bool isOutOfContact() const;

public slots:
    // The ground station has started tracking a different vehicle, or
    // its first one.
    void attachToActiveVehicle();

    // A new report landed, or the contact state moved.
    void refreshFromMapVehicle();

signals:
    void flightModeNameChanged();
    void isArmedChanged();
    void gpsFixDescriptionChanged();
    void satelliteCountChanged();
    void radioSignalPercentChanged();
    void batteryPercentChanged();
    void altitudeMetersAboveHomeChanged();
    void airspeedMetersPerSecondChanged();
    void rollDegreesChanged();
    void pitchDegreesChanged();
    void headingDegreesChanged();
    void secondsSinceLastReportChanged();
    void isOutOfContactChanged();

private slots:
    // Once a second, purely so the age on screen counts up. Nothing
    // here measures anything; the radio is what notices silence.
    void refreshReportAge();

private:
    // Numbers with a decimal point are compared against a small window
    // instead of an exact match. Two doubles almost never land on the
    // same value twice, so an exact check would announce every single
    // one and throw away the point of checking.
    static bool hasMovedEnough(double oldValue, double newValue);

    GroundControlStation *m_groundControlStation = nullptr;
    MapVehicle *m_mapVehicle = nullptr;

    QString m_flightModeName;
    bool m_isArmed = false;
    int m_gpsFixType = 0;
    int m_satelliteCount = 0;
    int m_radioSignalPercent = 0;
    int m_batteryPercent = 100;
    double m_altitudeMetersAboveHome = 0.0;
    double m_airspeedMetersPerSecond = 0.0;
    double m_rollDegrees = 0.0;
    double m_pitchDegrees = 0.0;
    double m_headingDegrees = 0.0;
    int m_secondsSinceLastReport = 0;
    bool m_isOutOfContact = false;

    QTimer m_reportAgeTimer;

    static constexpr double kSmallestChangeWorthTelling = 0.01;
};
