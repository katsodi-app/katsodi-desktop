import QtQuick
import Katsodi.App
import Katsodi.Ui

// The Katsodi mark (src/app/icon.svg) in the active palette.
Item {
    id: logo
    property int size: 40
    implicitWidth: size
    implicitHeight: size
    Rectangle {
        anchors.centerIn: parent
        width: logo.size * 0.875
        height: width
        radius: width * 0.3
        rotation: -8
        color: Theme.c.primary
    }
    Repeater {
        // Bars of the logo, in 64-unit space: x, top, bottom.
        model: [[16, 28, 36], [24, 22, 42], [32, 16, 48], [40, 26, 38], [48, 22, 42]]
        delegate: Rectangle {
            required property var modelData
            readonly property real s: logo.size / 64
            x: modelData[0] * s - width / 2
            y: modelData[1] * s
            width: Math.max(2, 3 * s)
            radius: width / 2
            height: (modelData[2] - modelData[1]) * s
            color: Theme.c.primaryForeground
        }
    }
}
