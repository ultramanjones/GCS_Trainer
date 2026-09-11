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
        // TODO (Lesson 1, Step 5): while the link is stale, this bar
        // should turn gray. Bind this color to vehicleModel.linkStale,
        // something like:
        //     color: vehicleModel.linkStale ? "#555555" : "#1b2a1e"
        color: "#1b2a1e"

        Behavior on color { ColorAnimation { duration: 300 } }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 16
            spacing: 16

            RowLayout {
                Layout.fillWidth: true
                spacing: 24

                // TODO (Lesson 1, Step 5): bind each Text below to the
                // matching vehicleModel property. One is done for you
                // as an example.

                Text {
                    text: "Mode: " + vehicleModel.flightMode
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    // TODO: text: vehicleModel.armed ? "ARMED" : "DISARMED"
                    text: "ARMED STATE HERE"
                    color: vehicleModel.armed ? "#e74c3c" : "#7f8c8d"
                    font.bold: true
                    font.pixelSize: 18
                }

                Text {
                    // TODO: text: "GPS: " + vehicleModel.gpsFixLabel + " (" + vehicleModel.satelliteCount + " sats)"
                    text: "GPS HERE"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    // TODO: text: "RSSI: " + vehicleModel.rssiPercent + "%"
                    text: "RSSI HERE"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    // TODO: text: "Batt: " + vehicleModel.batteryPercent + "%"
                    text: "BATTERY HERE"
                    color: "white"
                    font.pixelSize: 18
                }

                Text {
                    // TODO: text: "Link age: " + vehicleModel.linkAgeSeconds + "s"
                    text: "LINK AGE HERE"
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
