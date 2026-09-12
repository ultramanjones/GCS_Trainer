import QtQuick
import QtQuick.Controls

// The map. A grid in meters, the launch point, the trail of where the
// aircraft has already been, and the aircraft itself pointed the way
// it is flying.
//
// This is drawn by hand instead of using a street map, so the program
// needs no internet and no extra Qt modules to run.
Rectangle {
    id: flightMapPanel

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: GcsTheme.panelBackgroundColor
    clip: true

    // The view model counts up by one every time there is something
    // new to draw. Watching one number is cheaper than watching a
    // whole list.
    property int mapRevisionNumber: flightMapViewModel.mapRevisionNumber
    onMapRevisionNumberChanged: flightMapCanvas.requestPaint()

    // How much ground fits on screen, measured from the middle of the
    // view out to an edge.
    readonly property real halfRangeMeters: 340

    // The middle of the view, in meters east and north of the launch
    // point. It sits north of home because the practice circuit does.
    readonly property real viewCenterEastMeters: 0
    readonly property real viewCenterNorthMeters: 250

    Canvas {
        id: flightMapCanvas
        anchors.fill: parent

        onPaint: {
            // HOW THE MAP IS DRAWN.
            //
            // Everything the view model gives us is already in meters
            // east and north of the launch point. So the only math
            // here is turning meters into pixels.
            //
            // One meter is worth metersToPixels pixels. East grows to
            // the right, so screen x goes up with east. North grows
            // upward on a map but screen y grows DOWNWARD, so screen
            // y goes down when north goes up. That is the minus sign.

            var drawingContext = getContext("2d")
            drawingContext.reset()

            var canvasWidth = width
            var canvasHeight = height
            if (canvasWidth <= 0 || canvasHeight <= 0)
                return

            drawingContext.fillStyle = GcsTheme.panelBackgroundColor
            drawingContext.fillRect(0, 0, canvasWidth, canvasHeight)

            var metersToPixels = Math.min(canvasWidth, canvasHeight)
                               / (2 * flightMapPanel.halfRangeMeters)

            function screenXForEastMeters(eastMeters) {
                return canvasWidth / 2
                     + (eastMeters - flightMapPanel.viewCenterEastMeters) * metersToPixels
            }
            function screenYForNorthMeters(northMeters) {
                return canvasHeight / 2
                     - (northMeters - flightMapPanel.viewCenterNorthMeters) * metersToPixels
            }

            // The grid. One line every hundred meters.
            var gridStepMeters = 100
            drawingContext.strokeStyle = GcsTheme.mapGridColor
            drawingContext.lineWidth = 1

            var firstEast = Math.floor((flightMapPanel.viewCenterEastMeters - flightMapPanel.halfRangeMeters * 2) / gridStepMeters) * gridStepMeters
            var lastEast = flightMapPanel.viewCenterEastMeters + flightMapPanel.halfRangeMeters * 2
            for (var eastMeters = firstEast; eastMeters <= lastEast; eastMeters += gridStepMeters) {
                var gridX = screenXForEastMeters(eastMeters)
                drawingContext.beginPath()
                drawingContext.moveTo(gridX, 0)
                drawingContext.lineTo(gridX, canvasHeight)
                drawingContext.stroke()
            }

            var firstNorth = Math.floor((flightMapPanel.viewCenterNorthMeters - flightMapPanel.halfRangeMeters * 2) / gridStepMeters) * gridStepMeters
            var lastNorth = flightMapPanel.viewCenterNorthMeters + flightMapPanel.halfRangeMeters * 2
            for (var northMeters = firstNorth; northMeters <= lastNorth; northMeters += gridStepMeters) {
                var gridY = screenYForNorthMeters(northMeters)
                drawingContext.beginPath()
                drawingContext.moveTo(0, gridY)
                drawingContext.lineTo(canvasWidth, gridY)
                drawingContext.stroke()
            }

            // The trail of places the aircraft has already been.
            var trailPoints = flightMapViewModel.breadcrumbTrailPoints()
            if (trailPoints.length > 1) {
                drawingContext.strokeStyle = GcsTheme.mapTrailColor
                drawingContext.lineWidth = 2
                drawingContext.beginPath()
                drawingContext.moveTo(screenXForEastMeters(trailPoints[0].x),
                                      screenYForNorthMeters(trailPoints[0].y))
                for (var trailIndex = 1; trailIndex < trailPoints.length; ++trailIndex) {
                    drawingContext.lineTo(screenXForEastMeters(trailPoints[trailIndex].x),
                                          screenYForNorthMeters(trailPoints[trailIndex].y))
                }
                drawingContext.stroke()
            }

            // The launch point.
            var homeX = screenXForEastMeters(0)
            var homeY = screenYForNorthMeters(0)
            drawingContext.strokeStyle = GcsTheme.mapHomeColor
            drawingContext.lineWidth = 2
            drawingContext.strokeRect(homeX - 6, homeY - 6, 12, 12)
            drawingContext.fillStyle = GcsTheme.mapHomeColor
            drawingContext.font = "11px sans-serif"
            drawingContext.fillText("HOME", homeX + 10, homeY + 4)

            // The aircraft, drawn as an arrowhead pointed the way it
            // is flying. It is drawn pointing straight up and then
            // turned by the heading, because a heading of zero means
            // north, and north is up.
            var vehicleX = screenXForEastMeters(flightMapViewModel.vehicleEastMetersFromHome)
            var vehicleY = screenYForNorthMeters(flightMapViewModel.vehicleNorthMetersFromHome)
            var headingRadians = flightMapViewModel.vehicleHeadingDegrees * Math.PI / 180

            drawingContext.save()
            drawingContext.translate(vehicleX, vehicleY)
            drawingContext.rotate(headingRadians)
            drawingContext.fillStyle = GcsTheme.mapVehicleColor
            drawingContext.beginPath()
            drawingContext.moveTo(0, -13)
            drawingContext.lineTo(9, 10)
            drawingContext.lineTo(0, 5)
            drawingContext.lineTo(-9, 10)
            drawingContext.closePath()
            drawingContext.fill()
            drawingContext.restore()

            // A bar showing how long a hundred meters is on screen.
            var scaleBarPixels = gridStepMeters * metersToPixels
            var scaleBarY = canvasHeight - 22
            drawingContext.strokeStyle = GcsTheme.dimTextColor
            drawingContext.lineWidth = 2
            drawingContext.beginPath()
            drawingContext.moveTo(16, scaleBarY)
            drawingContext.lineTo(16 + scaleBarPixels, scaleBarY)
            drawingContext.stroke()
            drawingContext.fillStyle = GcsTheme.dimTextColor
            drawingContext.fillText("100 m", 16, scaleBarY - 6)
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        text: "MAP"
        color: GcsTheme.dimTextColor
        font.pixelSize: GcsTheme.smallLabelPixelSize
        font.letterSpacing: 1
    }

    Text {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 10
        horizontalAlignment: Text.AlignRight
        color: GcsTheme.dimTextColor
        font.pixelSize: 12
        text: flightMapViewModel.vehicleLatitudeDegrees.toFixed(6)
              + ",  " + flightMapViewModel.vehicleLongitudeDegrees.toFixed(6)
    }

    Button {
        id: clearTrailButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 110
        height: 30
        text: "Clear trail"
        onClicked: flightMapViewModel.clearBreadcrumbTrail()

        background: Rectangle {
            radius: 4
            color: clearTrailButton.pressed ? GcsTheme.panelBorderColor
                                            : GcsTheme.panelStaleBackgroundColor
            border.width: 1
            border.color: GcsTheme.panelBorderColor
        }
        contentItem: Text {
            text: clearTrailButton.text
            color: GcsTheme.normalTextColor
            font.pixelSize: 13
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
    }
}
