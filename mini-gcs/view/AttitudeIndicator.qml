import QtQuick

// The artificial horizon, the round instrument every pilot reads
// first. The brown half is the ground, the blue half is the sky, and
// the fixed yellow wings in the middle are the aircraft.
//
// The picture behind the wings moves. The wings never do. That is how
// a real one works, and it is why a pilot can read it in one glance.
Rectangle {
    id: attitudeIndicatorPanel

    radius: GcsTheme.panelCornerRadius
    border.width: 1
    border.color: GcsTheme.panelBorderColor
    color: GcsTheme.panelBackgroundColor

    // Copies of the three numbers this instrument draws. They are
    // copied into local properties so the canvas can be told to
    // repaint whenever any of them moves.
    property real rollDegrees: vehicleStatusViewModel.rollDegrees
    property real pitchDegrees: vehicleStatusViewModel.pitchDegrees
    property real headingDegrees: vehicleStatusViewModel.headingDegrees

    onRollDegreesChanged: horizonCanvas.requestPaint()
    onPitchDegreesChanged: horizonCanvas.requestPaint()

    Text {
        id: panelTitle
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 10
        text: "ATTITUDE"
        color: GcsTheme.dimTextColor
        font.pixelSize: GcsTheme.smallLabelPixelSize
        font.letterSpacing: 1
    }

    Canvas {
        id: horizonCanvas

        anchors.top: panelTitle.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: headingReadout.top
        anchors.margins: 8

        onPaint: {
            // HOW THIS DRAWING WORKS, step by step.
            //
            // 1. Everything is drawn inside a circle. A round clip is
            //    set first, so nothing spills past the instrument rim.
            // 2. The whole world is moved and turned under the
            //    circle, and the aircraft symbol is drawn on top
            //    afterward without any of that movement.
            // 3. Pitch moves the world straight up or down. Nose up
            //    means the horizon slides DOWN the screen, so a
            //    positive pitch shifts the drawing down by
            //    pitch times pixelsPerDegreeOfPitch.
            // 4. Roll turns the world the opposite way the aircraft
            //    turned. Bank to the right and the horizon tips so
            //    its right end rises. That is why the rotate call
            //    uses minus the roll angle.
            // 5. The ladder lines are painted on the moving world, so
            //    each one sits at its own angle above or below the
            //    horizon and rides along with it.

            var drawingContext = getContext("2d")
            drawingContext.reset()

            var canvasWidth = width
            var canvasHeight = height
            var centerX = canvasWidth / 2
            var centerY = canvasHeight / 2
            var instrumentRadius = Math.min(canvasWidth, canvasHeight) / 2 - 4

            if (instrumentRadius <= 0)
                return

            drawingContext.clearRect(0, 0, canvasWidth, canvasHeight)

            drawingContext.save()
            drawingContext.beginPath()
            drawingContext.arc(centerX, centerY, instrumentRadius, 0, Math.PI * 2)
            drawingContext.clip()

            var pixelsPerDegreeOfPitch = instrumentRadius / 35
            var pitchShiftPixels = attitudeIndicatorPanel.pitchDegrees * pixelsPerDegreeOfPitch
            var rollTurnRadians = -attitudeIndicatorPanel.rollDegrees * Math.PI / 180

            drawingContext.translate(centerX, centerY)
            drawingContext.rotate(rollTurnRadians)
            drawingContext.translate(0, pitchShiftPixels)

            var farEnough = instrumentRadius * 3

            drawingContext.fillStyle = GcsTheme.skyColor
            drawingContext.fillRect(-farEnough, -farEnough, farEnough * 2, farEnough)

            drawingContext.fillStyle = GcsTheme.groundColor
            drawingContext.fillRect(-farEnough, 0, farEnough * 2, farEnough)

            drawingContext.strokeStyle = GcsTheme.instrumentLineColor
            drawingContext.lineWidth = 2
            drawingContext.beginPath()
            drawingContext.moveTo(-farEnough, 0)
            drawingContext.lineTo(farEnough, 0)
            drawingContext.stroke()

            // The ladder. One line every ten degrees, longer on the
            // twenties so the eye can count them without reading.
            drawingContext.lineWidth = 1.5
            drawingContext.font = "10px sans-serif"
            drawingContext.fillStyle = GcsTheme.instrumentLineColor
            for (var ladderDegrees = -30; ladderDegrees <= 30; ladderDegrees += 10) {
                if (ladderDegrees === 0)
                    continue

                var ladderY = -ladderDegrees * pixelsPerDegreeOfPitch
                var ladderHalfWidth = (ladderDegrees % 20 === 0) ? 34 : 20

                drawingContext.beginPath()
                drawingContext.moveTo(-ladderHalfWidth, ladderY)
                drawingContext.lineTo(ladderHalfWidth, ladderY)
                drawingContext.stroke()

                drawingContext.fillText(Math.abs(ladderDegrees).toString(),
                                        ladderHalfWidth + 5, ladderY + 4)
            }

            drawingContext.restore()

            // The aircraft symbol. Fixed in the middle, never moves.
            drawingContext.strokeStyle = GcsTheme.mapVehicleColor
            drawingContext.lineWidth = 3
            drawingContext.beginPath()
            drawingContext.moveTo(centerX - 46, centerY)
            drawingContext.lineTo(centerX - 16, centerY)
            drawingContext.moveTo(centerX - 16, centerY)
            drawingContext.lineTo(centerX - 8, centerY + 9)
            drawingContext.moveTo(centerX + 46, centerY)
            drawingContext.lineTo(centerX + 16, centerY)
            drawingContext.moveTo(centerX + 16, centerY)
            drawingContext.lineTo(centerX + 8, centerY + 9)
            drawingContext.stroke()

            drawingContext.beginPath()
            drawingContext.arc(centerX, centerY, 3, 0, Math.PI * 2)
            drawingContext.stroke()

            // The rim.
            drawingContext.strokeStyle = GcsTheme.panelBorderColor
            drawingContext.lineWidth = 2
            drawingContext.beginPath()
            drawingContext.arc(centerX, centerY, instrumentRadius, 0, Math.PI * 2)
            drawingContext.stroke()
        }
    }

    Row {
        id: headingReadout
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottomMargin: 10
        spacing: 18

        Text {
            text: "HDG " + attitudeIndicatorPanel.headingDegrees.toFixed(0) + "\u00B0"
            color: GcsTheme.normalTextColor
            font.pixelSize: 15
            font.bold: true
        }
        Text {
            text: "ROLL " + attitudeIndicatorPanel.rollDegrees.toFixed(0) + "\u00B0"
            color: GcsTheme.dimTextColor
            font.pixelSize: 15
        }
        Text {
            text: "PITCH " + attitudeIndicatorPanel.pitchDegrees.toFixed(0) + "\u00B0"
            color: GcsTheme.dimTextColor
            font.pixelSize: 15
        }
    }
}
