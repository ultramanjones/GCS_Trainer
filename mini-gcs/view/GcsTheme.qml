pragma Singleton

import QtQuick

// One shared set of colors and sizes for the whole window.
//
// A singleton means there is exactly one of these for the whole
// program. Every panel reads from this one object, so the window
// stays consistent and a color change happens in one place.
// QGroundControl does the same thing with an object it calls qgcPal.
QtObject {
    readonly property color windowBackgroundColor: "#10141a"
    readonly property color panelBackgroundColor: "#1a212b"
    readonly property color panelBorderColor: "#2c3743"
    readonly property color panelStaleBackgroundColor: "#3a3f45"

    readonly property color normalTextColor: "#e6edf3"
    readonly property color dimTextColor: "#8b9bb0"

    readonly property color goodStateColor: "#4cc38a"
    readonly property color warningStateColor: "#e3b341"
    readonly property color criticalStateColor: "#e5534b"

    readonly property color skyColor: "#3d7ebd"
    readonly property color groundColor: "#7a5230"
    readonly property color instrumentLineColor: "#f2f4f7"

    readonly property color mapGridColor: "#31404c"
    readonly property color mapTrailColor: "#4cc38a"
    readonly property color mapVehicleColor: "#ffd166"
    readonly property color mapHomeColor: "#5aa9e6"

    readonly property int panelCornerRadius: 6
    readonly property int smallLabelPixelSize: 11
    readonly property int valuePixelSize: 19
}
