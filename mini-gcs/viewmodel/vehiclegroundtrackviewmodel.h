#pragma once

#include <QObject>
#include <QVariantList>

class GroundControlStation;
class MapVehicle;

// Everything the map panel draws.
//
// A ground track is the path a vehicle has taken over the ground. That
// is what this serves, which is why it is named for that and not for
// the panel it happens to feed.
//
// No map math happens here. Position arrives already measured in
// meters east and north of the launch point. This layer organizes; it
// does not calculate.
class VehicleGroundTrackViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double vehicleEastMetersFromHome READ vehicleEastMetersFromHome NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleNorthMetersFromHome READ vehicleNorthMetersFromHome NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleHeadingDegrees READ vehicleHeadingDegrees NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleLatitudeDegrees READ vehicleLatitudeDegrees NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleLongitudeDegrees READ vehicleLongitudeDegrees NOTIFY vehiclePositionChanged)
    Q_PROPERTY(bool isOutOfContact READ isOutOfContact NOTIFY isOutOfContactChanged)

    // Counts up by one every time the map has something new to draw.
    // The panel watches this one number and repaints. Watching a
    // number is cheaper than watching a whole list, and QML only needs
    // to know THAT something changed, not what.
    Q_PROPERTY(int mapRevisionNumber READ mapRevisionNumber NOTIFY mapRevisionNumberChanged)

public:
    explicit VehicleGroundTrackViewModel(GroundControlStation *groundControlStation,
                                         QObject *parent = nullptr);

    double vehicleEastMetersFromHome() const;
    double vehicleNorthMetersFromHome() const;
    double vehicleHeadingDegrees() const;
    double vehicleLatitudeDegrees() const;
    double vehicleLongitudeDegrees() const;
    bool isOutOfContact() const;
    int mapRevisionNumber() const;

    // The track as a list of points, where x is meters east of the
    // launch point and y is meters north of it.
    Q_INVOKABLE QVariantList groundTrackPoints() const;

    Q_INVOKABLE void clearGroundTrack();

public slots:
    void attachToActiveVehicle();
    void refreshFromMapVehicle();

signals:
    void vehiclePositionChanged();
    void isOutOfContactChanged();
    void mapRevisionNumberChanged();

private:
    GroundControlStation *m_groundControlStation = nullptr;
    MapVehicle *m_mapVehicle = nullptr;

    double m_vehicleEastMetersFromHome = 0.0;
    double m_vehicleNorthMetersFromHome = 0.0;
    double m_vehicleHeadingDegrees = 0.0;
    double m_vehicleLatitudeDegrees = 0.0;
    double m_vehicleLongitudeDegrees = 0.0;
    bool m_isOutOfContact = false;

    int m_mapRevisionNumber = 0;
};
