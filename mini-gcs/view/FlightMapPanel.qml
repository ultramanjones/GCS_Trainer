import QtQuick
import QtQuick.Controls

// The map. A grid in meters, the launch point, the trail of where the
// aircraft has already been, and the aircraft itself pointed the way
// it is flying.
//
// This is drawn by hand instead of using a street map, so the program
// needs no internet and no extra Qt modules to run.
//
// IMPORTANT, and the thing that broke this panel once already:
// a Canvas paints on its own render thread by default. Paint code must
// not reach out and call into a C++ object, because that object lives
// on the main thread. So this panel copies everything it needs into
// plain QML properties of its own, once per update, and the paint code
// only ever reads those copies. The render strategy is also set to
// Immediate, which keeps the painting on the main thread as a second
// layer of safety.
Rectangle {
    id: flightMapPanel

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: GcsTheme.panelBackgroundColor
    clip: true

    // --- The copies the drawing reads. Nothing else. ---
    property real vehicleEastMeters: 0
    property real vehicleNorthMeters: 0
    property real vehicleHeadingDegrees: 0
    property var groundTrack: []

    // The view model counts up by one every time there is something
    // new to draw. Watching one number is cheaper than watching a
    // whole list.
    property int mapRevisionNumber: vehicleGroundTrackViewModel.mapRevisionNumber

    onMapRevisionNumberChanged: {
        vehicleEastMeters = vehicleGroundTrackViewModel.vehicleEastMetersFromHome
        vehicleNorthMeters = vehicleGroundTrackViewModel.vehicleNorthMetersFromHome
        vehicleHeadingDegrees = vehicleGroundTrackViewModel.vehicleHeadingDegrees
        groundTrack = vehicleGroundTrackViewModel.groundTrackPoints()
        followTheVehicle()
        flightMapCanvas.requestPaint()
    }

    // The map follows the aircraft. The aircraft stays in the middle
    // and the ground slides underneath it.
    //
    // This is what QGroundControl does. Its Fly View map auto-centers
    // on the vehicle, and if you drag the map by hand it re-centers on
    // the vehicle again after a moment. A ground station map that
    // stays put is only usable until the aircraft flies past the edge,
    // which does not take long.
    //
    // Source: QGroundControl user guide, Fly View.
    // https://docs.qgroundcontrol.com/master/en/qgc-user-guide/fly_view/fly_view.html
    property real viewCenterEastMeters: 0
    property real viewCenterNorthMeters: 0

    // How much ground fits on screen, measured from the middle out to
    // the nearer edge. The wider side of the panel shows more than
    // this. At 340 a panel this shape shows a bit over a kilometer
    // across, which holds the whole practice pattern with room left.
    readonly property real halfRangeMeters: 340

    function followTheVehicle() {
        viewCenterEastMeters = vehicleEastMeters
        viewCenterNorthMeters = vehicleNorthMeters
    }

    Component.onCompleted: followTheVehicle()

    Canvas {
        id: flightMapCanvas
        anchors.fill: parent

        // Paint on the main thread. See the note at the top of the file.
        renderStrategy: Canvas.Immediate

        // A canvas can be handed a size of zero on the very first pass,
        // before the layout has settled. Ask for a repaint whenever the
        // size lands, or the first drawing never happens.
        Component.onCompleted: requestPaint()
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()

        onPaint: {
            // HOW THE MAP IS DRAWN.
            //
            // Everything here is already in meters east and north of
            // the launch point. So the only math is turning meters
            // into pixels.
            //
            // One meter is worth metersToPixels pixels. East grows to
            // the right, so screen x goes up with east. North grows
            // upward on a map but screen y grows DOWNWARD, so screen y
            // goes down when north goes up. That is the minus sign.

            var drawingContext = getContext("2d")
            drawingContext.reset()

            var canvasWidth = width
            var canvasHeight = height
            if (canvasWidth <= 0 || canvasHeight <= 0)
                return

            var halfRange = flightMapPanel.halfRangeMeters
            var centerEast = flightMapPanel.viewCenterEastMeters
            var centerNorth = flightMapPanel.viewCenterNorthMeters

            drawingContext.fillStyle = GcsTheme.panelBackgroundColor
            drawingContext.fillRect(0, 0, canvasWidth, canvasHeight)

            var metersToPixels = Math.min(canvasWidth, canvasHeight) / (2 * halfRange)

            function screenXForEastMeters(eastMeters) {
                return canvasWidth / 2 + (eastMeters - centerEast) * metersToPixels
            }
            function screenYForNorthMeters(northMeters) {
                return canvasHeight / 2 - (northMeters - centerNorth) * metersToPixels
            }

            // The grid. One line every hundred meters.
            var gridStepMeters = 100
            drawingContext.strokeStyle = GcsTheme.mapGridColor
            drawingContext.lineWidth = 1

            var gridReach = halfRange * 2
            var eastMeters
            var northMeters

            for (eastMeters = Math.floor((centerEast - gridReach) / gridStepMeters) * gridStepMeters;
                 eastMeters <= centerEast + gridReach;
                 eastMeters += gridStepMeters) {
                var gridX = screenXForEastMeters(eastMeters)
                drawingContext.beginPath()
                drawingContext.moveTo(gridX, 0)
                drawingContext.lineTo(gridX, canvasHeight)
                drawingContext.stroke()
            }

            for (northMeters = Math.floor((centerNorth - gridReach) / gridStepMeters) * gridStepMeters;
                 northMeters <= centerNorth + gridReach;
                 northMeters += gridStepMeters) {
                var gridY = screenYForNorthMeters(northMeters)
                drawingContext.beginPath()
                drawingContext.moveTo(0, gridY)
                drawingContext.lineTo(canvasWidth, gridY)
                drawingContext.stroke()
            }

            // The trail of places the aircraft has already been.
            var trailPoints = flightMapPanel.groundTrack
            if (trailPoints && trailPoints.length > 1) {
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
            // Below and right of the square, clear of the aircraft
            // symbol, which sits right on top of home when parked.
            drawingContext.fillText("HOME", homeX + 12, homeY + 22)

            // The aircraft, drawn as an arrowhead pointed the way it is
            // flying. It is drawn pointing straight up and then turned
            // by the heading, because a heading of zero means north,
            // and north is up.
            var vehicleX = screenXForEastMeters(flightMapPanel.vehicleEastMeters)
            var vehicleY = screenYForNorthMeters(flightMapPanel.vehicleNorthMeters)
            var headingRadians = flightMapPanel.vehicleHeadingDegrees * Math.PI / 180

            drawingContext.save()
            drawingContext.translate(vehicleX, vehicleY)
            drawingContext.rotate(headingRadians)
            drawingContext.fillStyle = GcsTheme.mapVehicleColor
            drawingContext.beginPath()
            drawingContext.moveTo(0, -14)
            drawingContext.lineTo(10, 11)
            drawingContext.lineTo(0, 5)
            drawingContext.lineTo(-10, 11)
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
        text: vehicleGroundTrackViewModel.vehicleLatitudeDegrees.toFixed(6)
              + ",  " + vehicleGroundTrackViewModel.vehicleLongitudeDegrees.toFixed(6)
    }

    Button {
        id: clearTrailButton
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 10
        width: 110
        height: 30
        text: "Clear trail"
        onClicked: {
            vehicleGroundTrackViewModel.clearGroundTrack()
            flightMapPanel.groundTrack = []
            flightMapCanvas.requestPaint()
        }

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
