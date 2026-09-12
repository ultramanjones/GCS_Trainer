#pragma once

#include <QList>
#include <QObject>
#include <QPointF>
#include <QString>

#include "model/vehicleattitude.h"
#include "model/vehiclebattery.h"
#include "model/vehicleposition.h"
#include "model/vehiclesitrep.h"

// What the ground station believes about one vehicle.
//
// This is not the aircraft. It is the picture of the aircraft, built
// only from messages that arrived. In a real deployment this is the
// only vehicle object that exists anywhere in the program.
//
// It knows one thing the aircraft can never know: whether it is out of
// contact. The aircraft has no idea it has gone quiet. The radio
// notices the silence and tells this object, and this object owns the
// fact from then on. It has to live here and not on the link, because
// one link can carry several vehicles and you need to know which one
// went quiet.
//
// QGroundControl calls its version of this Vehicle.
class MapVehicle : public QObject
{
    Q_OBJECT

public:
    explicit MapVehicle(int vehicleIdentifier, QObject *parent = nullptr);

    int vehicleIdentifier() const;

    QString flightModeName() const;
    bool isArmed() const;
    int gpsFixType() const;
    int satelliteCount() const;
    int radioSignalPercent() const;
    double airspeedMetersPerSecond() const;
    double groundspeedMetersPerSecond() const;

    // The last position, attitude and charge that actually arrived.
    // While out of contact these keep their old values on purpose, and
    // the out of contact flag is what says they are old.
    const VehiclePosition &lastConfirmedPosition() const;
    const VehicleAttitude &lastConfirmedAttitude() const;
    const VehicleBattery &lastConfirmedBattery() const;

    bool isOutOfContact() const;
    int secondsSinceLastReport() const;

    // Where this vehicle has been, in meters east and north of the
    // launch point. The ground station's own record, not the
    // aircraft's.
    QList<QPointF> groundTrackPoints() const;

public slots:
    void applySitRep(VehicleSitRep sitRep);
    void markOutOfContact();
    void markBackInContact();
    void clearGroundTrack();

signals:
    // One signal for a whole new report. The view models do their own
    // change checking, because deciding what is worth telling the
    // screen is their job, not this object's.
    void sitRepApplied();

    void flightModeChanged(QString newFlightModeName);
    void contactStateChanged();
    void groundTrackChanged();

private:
    int m_vehicleIdentifier = 0;

    QString m_flightModeName;
    bool m_isArmed = false;
    int m_gpsFixType = 0;
    int m_satelliteCount = 0;
    int m_radioSignalPercent = 0;
    double m_airspeedMetersPerSecond = 0.0;
    double m_groundspeedMetersPerSecond = 0.0;

    VehiclePosition m_lastConfirmedPosition;
    VehicleAttitude m_lastConfirmedAttitude;
    VehicleBattery m_lastConfirmedBattery;

    bool m_isOutOfContact = false;
    qint64 m_lastReportArrivedAtMilliseconds = 0;

    QList<QPointF> m_groundTrackPoints;

    // A new point is only kept once the vehicle has moved this far.
    // Without that the track fills with thousands of points on top of
    // each other and the redraw gets slower every second.
    static constexpr double kMinimumMetersBetweenTrackPoints = 4.0;

    // Oldest points fall off past this many, so a long flight cannot
    // eat all the memory.
    static constexpr int kMaximumTrackPoints = 900;
};
