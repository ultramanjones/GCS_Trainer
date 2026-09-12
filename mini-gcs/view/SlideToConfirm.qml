import QtQuick

// A control you have to drag all the way across before it does
// anything.
//
// Cutting the motors on a flying aircraft should not be one careless
// click away. A plain button, even a big red one, can be hit by a
// brushed sleeve. Dragging a handle the whole width of the control
// costs a deliberate second, which is what this control is for.
//
// This is not the same thing as graying a button out. Nothing here is
// forbidden. It is made slow on purpose.
Item {
    id: slideToConfirm

    property string labelText: "SLIDE TO CONFIRM"
    property color accentColor: GcsTheme.criticalStateColor

    // How far across counts as all the way. Not the whole width,
    // because the last few pixels are fussy to hit, and fussy is not
    // the same thing as safe.
    readonly property real fractionThatCounts: 0.9

    signal confirmed()

    implicitHeight: 44

    Rectangle {
        id: track

        anchors.fill: parent
        radius: height / 2
        color: GcsTheme.panelStaleBackgroundColor
        border.width: 1
        border.color: slideToConfirm.accentColor

        readonly property real handleInset: 3
        readonly property real handleTravel:
            Math.max(1, width - handle.width - (handleInset * 2))

        Text {
            anchors.centerIn: parent
            text: slideToConfirm.labelText
            color: slideToConfirm.accentColor
            font.pixelSize: 12
            font.bold: true
            font.letterSpacing: 1

            // Fades out as the handle crosses it, so the person can see
            // how far along they are without reading anything.
            opacity: 1 - ((handle.x - track.handleInset) / track.handleTravel)
        }

        Rectangle {
            id: handle

            width: track.height - (track.handleInset * 2)
            height: width
            radius: width / 2
            x: track.handleInset
            y: track.handleInset
            color: slideToConfirm.accentColor

            Text {
                anchors.centerIn: parent
                text: "▶"
                color: GcsTheme.panelBackgroundColor
                font.pixelSize: 13
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.OpenHandCursor

                drag.target: handle
                drag.axis: Drag.XAxis
                drag.minimumX: track.handleInset
                drag.maximumX: track.handleInset + track.handleTravel

                onReleased: {
                    var howFar = (handle.x - track.handleInset) / track.handleTravel
                    if (howFar >= slideToConfirm.fractionThatCounts)
                        slideToConfirm.confirmed()

                    // The handle always goes home, whether it fired or
                    // not. A control left sitting at the far end looks
                    // like it is still armed.
                    handleGoesHome.start()
                }
            }
        }

        NumberAnimation {
            id: handleGoesHome
            target: handle
            property: "x"
            to: track.handleInset
            duration: 160
            easing.type: Easing.OutCubic
        }
    }
}
