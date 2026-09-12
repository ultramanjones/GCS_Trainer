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
    height: 900
    minimumWidth: 1000

    // Tall enough that the command panel always fits without being cut
    // off at the bottom. Add the parts up: 20 of margin, a 64 status
    // bar, the smallest useful horizon at 190, the command panel's own
    // height, a 110 message list, and 30 of gaps. Every time the
    // command panel grows, this number has to grow with it.
    minimumHeight: 820

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

            // The map takes all the room left over, and it has to ASK
            // for a real width to get it.
            //
            // This one cost an evening. A layout hands out leftover
            // space in proportion to what each item asked for. A plain
            // Rectangle asks for zero. Zero against the instrument
            // column's 330 means the column takes everything, and this
            // panel came out three pixels wide. It was drawing the
            // whole time, into a sliver nobody could see.
            //
            // Layout.fillWidth does not fix that on its own. All it
            // does is make an item eligible for a share. The preferred
            // width is what decides how big the share is.
            FlightMapPanel {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.preferredWidth: 900
                Layout.minimumWidth: 420
            }

            // The instrument column is a side panel of one fixed width,
            // the way a real ground station lays one out. It never
            // grows, so the map gets every pixel the window gains.
            ColumnLayout {
                Layout.fillWidth: false
                Layout.fillHeight: true
                Layout.preferredWidth: 330
                Layout.minimumWidth: 330
                Layout.maximumWidth: 330
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
