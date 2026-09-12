import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The whole window.
//
// The view holds no rules and does no math. It shows what it is given
// and passes along what the person did. Every number on this screen
// comes from a view model that main.cpp handed to QML.
ApplicationWindow {
    id: mainWindow

    width: 1280
    height: 880
    minimumWidth: 1000

    // Tall enough that the command panel always fits without being
    // cut off at the bottom. Work it out from the parts: the status
    // bar, the message list, the smallest useful horizon, the command
    // panel's own height, and the gaps between them.
    minimumHeight: 780

    visible: true
    title: "Mini GCS - practice ground control station"

    color: GcsTheme.windowBackgroundColor

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        TelemetryStatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            FlightMapPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
            }

            ColumnLayout {
                Layout.preferredWidth: 330
                Layout.fillHeight: true
                spacing: 10

                // The horizon is the flexible one. It is a circle, so it
                // looks right at any size and can give up room when the
                // window is short.
                AttitudeIndicator {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 320
                    Layout.minimumHeight: 190
                }

                // The command panel is NOT flexible. It asks for exactly
                // the height its buttons need and never less, so the
                // bottom row can never be cut off.
                CommandPanel {
                    Layout.fillWidth: true
                    Layout.preferredHeight: implicitHeight
                    Layout.minimumHeight: implicitHeight
                }
            }
        }

        AlertListPanel {
            Layout.fillWidth: true
            Layout.preferredHeight: 160
            Layout.minimumHeight: 110
        }
    }
}
