import QtQuick
import Katsodi.App
import Katsodi.Ui

// The window's backdrop: colour fields in the active palette that slowly
// flow into each other (shaders/backdrop.frag), tinted by the open room's
// colours, which ease in when a room opens.
//
// Cost control: every step redraws the whole window, so the fields move
// slowly and step only 10 times a second (a few pixels of a blurred field:
// not visible as steps), and not at all while the window is in the
// background or minimized, motion is reduced (Windows setting) or Low
// resource mode is on. Then the fields stand still.
Item {
    id: backdrop
    property color tintA: Theme.c.accent
    property color tintB: Theme.c.info
    property bool running: true
    Behavior on tintA { ColorAnimation { duration: K.slow * 2 } }
    Behavior on tintB { ColorAnimation { duration: K.slow * 2 } }

    Rectangle { anchors.fill: parent; color: Theme.c.background }

    ShaderEffect {
        id: fields
        anchors.fill: parent
        property real time: 7.0
        property real aspect: width / Math.max(1, height)
        property real strength: Theme.dark ? 0.30 : 0.20
        property color base: Theme.c.background
        property color tintA: backdrop.tintA
        property color tintB: backdrop.tintB
        property color tintC: Theme.c.primary
        fragmentShader: "qrc:/katsodi/shaders/backdrop.frag.qsb"
        Behavior on strength { NumberAnimation { duration: K.slow } }
    }

    Timer {
        interval: 100
        repeat: true
        running: backdrop.running && !K.still
        onTriggered: fields.time += 0.05
    }
}
