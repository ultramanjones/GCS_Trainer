import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The buttons that send orders to the vehicle.
//
// No button is ever grayed out. The house rules say a guardrail
// advises and never decides. So every button always sends, the
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
                onClicked: commandPanelViewModel.requestArm()
            }
            CommandButton {
                text: "Disarm"
                onClicked: commandPanelViewModel.requestDisarm()
            }
        }

        CommandButton {
            text: "Launch"
            accentColor: GcsTheme.goodStateColor
            onClicked: commandPanelViewModel.requestLaunch()
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            CommandButton {
                text: "Return"
                onClicked: commandPanelViewModel.requestReturnToLaunch()
            }
            CommandButton {
                text: "Land"
                onClicked: commandPanelViewModel.requestLand()
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
                onClicked: commandPanelViewModel.requestDropLink()
            }
            CommandButton {
                text: "Reconnect"
                onClicked: commandPanelViewModel.requestReconnectLink()
            }
        }

        // What happened to the last order. This is the whole point of
        // waiting for an answer instead of assuming one.
        Text {
            Layout.fillWidth: true
            text: commandPanelViewModel.lastCommandResultText === ""
                  ? "No orders sent yet."
                  : commandPanelViewModel.lastCommandResultText
            color: commandPanelViewModel.isWaitingForAcknowledgment
                   ? GcsTheme.warningStateColor
                   : GcsTheme.normalTextColor
            font.pixelSize: 13
            wrapMode: Text.WordWrap
        }
    }
}
