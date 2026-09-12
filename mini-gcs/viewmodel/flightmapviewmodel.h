#pragma once

#include <QList>
#include <QObject>
#include <QPointF>
#include <QVariantList>

#include "model/telemetrysnapshot.h"

// Everything the map panel draws.
//
// It holds where the vehicle is, which way it is pointed, and the
// trail of places it has already been. All of it in meters east and
// north of the launch point, because that is what the drawing code
// wants and the back end already worked it out.
//
// No map math happens here. This layer organizes, it does not
// calculate.
class FlightMapViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(double vehicleEastMetersFromHome READ vehicleEastMetersFromHome NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleNorthMetersFromHome READ vehicleNorthMetersFromHome NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleHeadingDegrees READ vehicleHeadingDegrees NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleLatitudeDegrees READ vehicleLatitudeDegrees NOTIFY vehiclePositionChanged)
    Q_PROPERTY(double vehicleLongitudeDegrees READ vehicleLongitudeDegrees NOTIFY vehiclePositionChanged)

    // Counts up by one every time the map has something new to draw.
    // The map panel watches this number and repaints when it moves.
    // A plain counter is used instead of handing QML a list, because
    // QML only needs to know THAT something changed, not what.
    Q_PROPERTY(int mapRevisionNumber READ mapRevisionNumber NOTIFY mapRevisionNumberChanged)

public:
    explicit FlightMapViewModel(QObject *parent = nullptr);

    double vehicleEastMetersFromHome() const;
    double vehicleNorthMetersFromHome() const;
    double vehicleHeadingDegrees() const;
    double vehicleLatitudeDegrees() const;
    double vehicleLongitudeDegrees() const;
    int mapRevisionNumber() const;

    // Hands the drawing code the trail as a list of points, where x
    // is meters east of home and y is meters north of home.
    Q_INVOKABLE QVariantList breadcrumbTrailPoints() const;

    Q_INVOKABLE void clearBreadcrumbTrail();

public slots:
    void applyTelemetrySnapshot(TelemetrySnapshot snapshot);

signals:
    void vehiclePositionChanged();
    void mapRevisionNumberChanged();

private:
    double m_vehicleEastMetersFromHome = 0.0;
    double m_vehicleNorthMetersFromHome = 0.0;
    double m_vehicleHeadingDegrees = 0.0;
    double m_vehicleLatitudeDegrees = 0.0;
    double m_vehicleLongitudeDegrees = 0.0;

    QList<QPointF> m_breadcrumbTrailPoints;
    int m_mapRevisionNumber = 0;

    // A new crumb is only kept when the vehicle has actually moved
    // this far. Without that check the trail would fill up with
    // thousands of points sitting on top of each other and the redraw
    // would get slower every second.
    static constexpr double kMinimumMetersBetweenCrumbs = 4.0;

    // Oldest crumbs get thrown away past this many, so a long flight
    // cannot eat all the memory.
    static constexpr int kMaximumTrailPoints = 900;
};
