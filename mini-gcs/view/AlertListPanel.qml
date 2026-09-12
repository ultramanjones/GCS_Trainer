import QtQuick
import QtQuick.Controls

// The message list. Newest on top.
//
// The rows come from a C++ list model. QML asks that model how many
// rows there are and then asks for one named piece of a row at a
// time. Those named pieces are the roles, and their names are set in
// the C++ file: alertTimeText, alertSeverityName, alertSeverityValue,
// and alertMessageText.
Rectangle {
    id: alertListPanel

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: GcsTheme.panelBackgroundColor
    clip: true

    // Picks the color for one row from how bad the message is. The
    // numbers match the AlertSeverity values in the C++ model:
    // 0 is information, 1 is a warning, 2 is critical.
    function colorForSeverityValue(severityValue) {
        if (severityValue === 2)
            return GcsTheme.criticalStateColor
        if (severityValue === 1)
            return GcsTheme.warningStateColor
        return GcsTheme.dimTextColor
    }

    Text {
        id: panelTitle
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        text: "MESSAGES"
        color: GcsTheme.dimTextColor
        font.pixelSize: GcsTheme.smallLabelPixelSize
        font.letterSpacing: 1
    }

    Button {
        id: clearAlertsButton
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 6
        width: 70
        height: 24
        text: "Clear"
        onClicked: alertListViewModel.clearAllAlerts()

        background: Rectangle {
            radius: 4
            color: clearAlertsButton.pressed ? GcsTheme.panelBorderColor
                                             : GcsTheme.panelStaleBackgroundColor
            border.width: 1
            border.color: GcsTheme.panelBorderColor
        }
        contentItem: Text {
            text: clearAlertsButton.text
            color: GcsTheme.normalTextColor
            font.pixelSize: 12
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }

    ListView {
        id: alertListView

        anchors.top: panelTitle.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        anchors.topMargin: 4

        clip: true
        model: alertListViewModel
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar { }

        delegate: Row {
            id: oneAlertRow

            required property string alertTimeText
            required property string alertSeverityName
            required property int alertSeverityValue
            required property string alertMessageText

            width: alertListView.width
            height: 20
            spacing: 10

            Text {
                text: oneAlertRow.alertTimeText
                color: GcsTheme.dimTextColor
                font.pixelSize: 13
                font.family: "Consolas"
                width: 66
            }
            Text {
                text: oneAlertRow.alertSeverityName
                color: alertListPanel.colorForSeverityValue(oneAlertRow.alertSeverityValue)
                font.pixelSize: 13
                font.bold: true
                font.family: "Consolas"
                width: 44
            }
            Text {
                text: oneAlertRow.alertMessageText
                color: GcsTheme.normalTextColor
                font.pixelSize: 13
            }
        }
    }
}
