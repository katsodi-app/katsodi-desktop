import QtQuick
import QtQuick.Shapes
import Katsodi.App
import Katsodi.Ui

// The window's backdrop: the overview's night-sky photograph under a veil
// that settles into the palette background, plus Liner's ambient glow in
// the room's colours, easing to new tones when a room opens. Light
// palettes and low-resource mode drop the photograph. Nothing animates
// while the window is idle.
Item {
    id: backdrop
    property color tintA: Theme.c.accent
    property color tintB: Theme.c.info
    Behavior on tintA { ColorAnimation { duration: K.slow * 2 } }
    Behavior on tintB { ColorAnimation { duration: K.slow * 2 } }
    readonly property bool photo: Theme.dark && !Prefs.lowResources

    Rectangle { anchors.fill: parent; color: Theme.c.background }

    Image {
        anchors.fill: parent
        visible: backdrop.photo
        source: backdrop.photo ? editorialBase + "space-1600.webp" : ""
        asynchronous: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.min(1600, Math.ceil(width * Screen.devicePixelRatio))
        opacity: status === Image.Ready ? 0.9 : 0
        Behavior on opacity { NumberAnimation { duration: K.slow } }
    }

    // Glows: two soft radial lights in the tint colours.
    Repeater {
        model: [
            { tint: "a", x: 0.78, y: 0.12, size: 0.75, alpha: 0.22 },
            { tint: "b", x: 0.12, y: 0.78, size: 0.65, alpha: 0.14 }
        ]
        delegate: Shape {
            id: glow
            required property var modelData
            required property int index
            readonly property real d: Math.max(backdrop.width, backdrop.height) * modelData.size
            width: d
            height: d
            x: backdrop.width * modelData.x - d / 2
            y: backdrop.height * modelData.y - d / 2
            preferredRendererType: Shape.GeometryRenderer
            ShapePath {
                strokeWidth: -1
                fillGradient: RadialGradient {
                    centerX: glow.d / 2; centerY: glow.d / 2
                    centerRadius: glow.d / 2
                    focalX: centerX; focalY: centerY
                    GradientStop { position: 0; color: Qt.alpha(glow.modelData.tint === "a" ? backdrop.tintA : backdrop.tintB, glow.modelData.alpha) }
                    GradientStop { position: 1; color: "transparent" }
                }
                PathRectangle { x: 0; y: 0; width: glow.d; height: glow.d }
            }
            // No endless drift: an idle window must not redraw every frame.
            // The glow eases to a new colour when a room with other tones opens.
        }
    }

    // Veil: the photo shows at the top and settles into the background.
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Qt.alpha(Theme.c.background, backdrop.photo ? 0.30 : 0.0) }
            GradientStop { position: 0.45; color: Qt.alpha(Theme.c.background, backdrop.photo ? 0.72 : 0.0) }
            GradientStop { position: 1.0; color: Qt.alpha(Theme.c.background, backdrop.photo ? 0.94 : 0.0) }
        }
    }
}
