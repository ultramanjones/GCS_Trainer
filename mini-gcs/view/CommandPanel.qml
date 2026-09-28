import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The buttons that send orders to the vehicle.
//
// No button is ever grayed out. A guardrail should advise, never
// decide. So every button always sends, the
// vehicle answers yes or no, and the answer is printed here and in
// the alert list. The operator is never left guessing why a button
// would not work.
Rectangle {
    id: commandPanel

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: GcsTheme.panelBackgroundColor

    // The panel tells the window how tall it needs to be, instead of
    // taking whatever is left over. The column inside is anchored to
    // the top three sides only, so its height is its own content
    // height, and this panel is that plus the margins above and below.
    readonly property int panelMargin: 10
    implicitHeight: commandColumn.implicitHeight + (panelMargin * 2)

    // One button, styled once, used for all of them.
    component CommandButton: Button {
        id: oneCommandButton

        property color accentColor: GcsTheme.panelBorderColor

        Layout.fillWidth: true
        Layout.preferredHeight: 40

        background: Rectangle {
            radius: 4
            color: oneCommandButton.pressed
                   ? oneCommandButton.accentColor
                   : GcsTheme.panelStaleBackgroundColor
            border.width: 1
            border.color: oneCommandButton.accentColor
        }

        contentItem: Text {
            text: oneCommandButton.text
            color: GcsTheme.normalTextColor
            font.pixelSize: 14
            font.bold: true
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ColumnLayout {
        id: commandColumn

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: commandPanel.panelMargin
        spacing: 8

        Text {
            text: "COMMANDS"
            color: GcsTheme.dimTextColor
            font.pixelSize: GcsTheme.smallLabelPixelSize
            font.letterSpacing: 1
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CommandButton {
                text: "Arm"
                accentColor: GcsTheme.criticalStateColor
                onClicked: vehicleCommandViewModel.requestArm()
            }
            CommandButton {
                text: "Disarm"
                onClicked: vehicleCommandViewModel.requestDisarm()
            }
        }

        CommandButton {
            text: "Launch"
            accentColor: GcsTheme.goodStateColor
            onClicked: vehicleCommandViewModel.requestLaunch()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CommandButton {
                text: "Return"
                onClicked: vehicleCommandViewModel.requestReturnToHome()
            }
            CommandButton {
                text: "Land"
                onClicked: vehicleCommandViewModel.requestLand()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: GcsTheme.panelBorderColor
        }

        Text {
            text: "RADIO"
            color: GcsTheme.dimTextColor
            font.pixelSize: GcsTheme.smallLabelPixelSize
            font.letterSpacing: 1
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CommandButton {
                text: "Drop link"
                accentColor: GcsTheme.warningStateColor
                onClicked: vehicleCommandViewModel.requestRadioUnplug()
            }
            CommandButton {
                text: "Reconnect"
                onClicked: vehicleCommandViewModel.requestRadioReconnect()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: GcsTheme.panelBorderColor
        }

        Text {
            text: "EMERGENCY"
            color: GcsTheme.criticalStateColor
            font.pixelSize: GcsTheme.smallLabelPixelSize
            font.letterSpacing: 1
        }

        // Cutting the motors in the air is never refused by the
        // vehicle, so the guard has to live in the control itself.
        SlideToConfirm {
            Layout.fillWidth: true
            Layout.preferredHeight: 44
            labelText: "SLIDE TO CUT MOTORS"
            onConfirmed: vehicleCommandViewModel.requestEmergencyStop()
        }

        // What happened to the last order. This is the whole point of
        // waiting for an answer instead of assuming one.
        Text {
            Layout.fillWidth: true
            text: vehicleCommandViewModel.lastCommandResultText === ""
                  ? "No orders sent yet."
                  : vehicleCommandViewModel.lastCommandResultText
            color: GcsTheme.normalTextColor
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
    }
}
