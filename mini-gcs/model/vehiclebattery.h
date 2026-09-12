#pragma once

// What is left in the battery.
//
// A plain value. It copies and has no Qt object in it.
class VehicleBattery
{
public:
    VehicleBattery();

    double chargePercent() const;

    // Take charge out at this rate for this long. Charge never goes
    // below empty and never above full.
    void drainFor(double stepSeconds, double percentPerSecond);

    void setChargePercent(double percent);

private:
    double m_chargePercent = 100.0;
};
