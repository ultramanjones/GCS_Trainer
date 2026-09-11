import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The view. It shows what it is given and passes along what the
// person did — nothing here computes anything. Every value on screen
// comes straight from the vehicleModel context property that main.cpp
// set up.
ApplicationWindow {
    id: window
    width: 640
    height: 200
    visible: true
    title: "GCS Trainer — Lesson 1: Telemetry Bar"

    Rectangle {
        anchors.fill: parent
        color: vehicleModel.linkStale ? "#555555" : "#1b2a1e"

        Behavior on color { ColorAnimation { duration: 300 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 24

                Text {
                    text: "Mode: " + vehicleModel.flightMode
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    text: vehicleModel.armed ? "ARMED" : "DISARMED"
                    color: vehicleModel.armed ? "#e74c3c" : "#7f8c8d"
                    font.bold: true
                    font.pixelSize: 18
                }

                Text {
                    text: "GPS: " + vehicleModel.gpsFixLabel + " (" + vehicleModel.satelliteCount + " sats)"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    text: "RSSI: " + vehicleModel.rssiPercent + "%"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    text: "Batt: " + vehicleModel.batteryPercent + "%"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    text: "Link age: " + vehicleModel.linkAgeSeconds + "s"
                    color: vehicleModel.linkStale ? "#e74c3c" : "white"
                    font.pixelSize: 18
                }
            }

            Item { Layout.fillHeight: true }

            Button {
                text: "Drop link"
                onClicked: vehicleModel.dropLink()
            }
        }
    }
}
