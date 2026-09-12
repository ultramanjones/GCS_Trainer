import QtQuick
import QtQuick.Layouts

// The strip across the top. Flight mode, armed state, GPS, radio,
// battery, altitude, speed, and how old the newest reading is.
//
// The whole strip turns gray when the link goes quiet. A ground
// station that keeps showing the last good battery number after the
// radio died is lying to the operator.
Rectangle {
    id: telemetryStatusBar

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: vehicleStatusViewModel.isLinkStale
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
            valueText: vehicleStatusViewModel.secondsSinceLastSnapshot + " s ago"
            valueColor: vehicleStatusViewModel.isLinkStale
                        ? GcsTheme.criticalStateColor
                        : GcsTheme.normalTextColor
        }
    }
}
