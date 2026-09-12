import QtQuick
import QtQuick.Layouts

// The strip across the top. Flight mode, armed state, GPS, radio,
// battery, altitude, speed, and how old the newest reading is.
//
// The whole strip turns gray when the link goes quiet, so a stale
// reading cannot be mistaken for a current one. This is crucial: an
// operator acting on a battery number that is minutes old has no way
// to know it is old.
Rectangle {
    id: telemetryStatusBar

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: vehicleStatusViewModel.isOutOfContact
           ? GcsTheme.panelStaleBackgroundColor
           : GcsTheme.panelBackgroundColor

    Behavior on color { ColorAnimation { duration: 300 } }

    // One readout: a small label on top, the value under it.
    component StatusReadout: ColumnLayout {
        property string labelText: ""
        property string valueText: ""
        property color valueColor: GcsTheme.normalTextColor

        spacing: 2

        Text {
            text: labelText
            color: GcsTheme.dimTextColor
            font.pixelSize: GcsTheme.smallLabelPixelSize
            font.letterSpacing: 1
        }
        Text {
            text: valueText
            color: valueColor
            font.pixelSize: GcsTheme.valuePixelSize
            font.bold: true
        }
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        spacing: 24

        StatusReadout {
            labelText: "MODE"
            valueText: vehicleStatusViewModel.flightModeName === ""
                       ? "---"
                       : vehicleStatusViewModel.flightModeName
        }

        StatusReadout {
            labelText: "ARMED"
            valueText: vehicleStatusViewModel.isArmed ? "ARMED" : "DISARMED"
            valueColor: vehicleStatusViewModel.isArmed
                        ? GcsTheme.criticalStateColor
                        : GcsTheme.dimTextColor
        }

        StatusReadout {
            labelText: "GPS"
            valueText: vehicleStatusViewModel.gpsFixDescription
                       + "  " + vehicleStatusViewModel.satelliteCount + " sats"
        }

        StatusReadout {
            labelText: "RADIO"
            valueText: vehicleStatusViewModel.radioSignalPercent + "%"
            valueColor: vehicleStatusViewModel.radioSignalPercent < 30
                        ? GcsTheme.criticalStateColor
                        : GcsTheme.normalTextColor
        }

        StatusReadout {
            labelText: "BATTERY"
            valueText: vehicleStatusViewModel.batteryPercent + "%"
            valueColor: vehicleStatusViewModel.batteryPercent <= 25
                        ? GcsTheme.warningStateColor
                        : GcsTheme.normalTextColor
        }

        StatusReadout {
            labelText: "ALTITUDE"
            valueText: vehicleStatusViewModel.altitudeMetersAboveHome.toFixed(0) + " m"
        }

        StatusReadout {
            labelText: "AIRSPEED"
            valueText: vehicleStatusViewModel.airspeedMetersPerSecond.toFixed(1) + " m/s"
        }

        Item { Layout.fillWidth: true }

        StatusReadout {
            labelText: "LAST UPDATE"
            valueText: vehicleStatusViewModel.secondsSinceLastReport + " s ago"
            valueColor: vehicleStatusViewModel.isOutOfContact
                        ? GcsTheme.criticalStateColor
                        : GcsTheme.normalTextColor
        }
    }
}
