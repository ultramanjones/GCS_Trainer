#include "model/vehiclebattery.h"

#include <QtGlobal>

VehicleBattery::VehicleBattery()
{
}

double VehicleBattery::chargePercent() const
{
    return m_chargePercent;
}

void VehicleBattery::drainFor(double stepSeconds, double percentPerSecond)
{
    setChargePercent(m_chargePercent - (percentPerSecond * stepSeconds));
}

void VehicleBattery::setChargePercent(double percent)
{
    m_chargePercent = qBound(0.0, percent, 100.0);
}
