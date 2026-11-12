import QtQuick
import QtQuick.Shapes
import Katsodi.App

// Artwork with the website's fallback: initials on the selected tint.
// Decoded at the size it is shown, asynchronously, and only when visible.
// Rounded corners are drawn as four small masks in the colour behind the
// poster (cornerColor) instead of an offscreen layer per image.
Rectangle {
    id: poster
    property url source
    property string initials: "K"
    property color cornerColor: Theme.c.background
    radius: 12
    color: Theme.c.selected

    Text {
        anchors.centerIn: parent
        visible: image.status !== Image.Ready
        text: poster.initials
        color: Theme.c.accent
        font.pixelSize: Math.min(poster.width, poster.height) * 0.28
        font.weight: Font.Bold
    }
    Image {
        id: image
        anchors.fill: parent
        source: poster.visible ? poster.source : ""
        asynchronous: true
        cache: true
        fillMode: Image.PreserveAspectCrop
        sourceSize.width: Math.ceil(poster.width * Screen.devicePixelRatio)
        sourceSize.height: Math.ceil(poster.height * Screen.devicePixelRatio)
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { enabled: !Prefs.reduceMotion; NumberAnimation { duration: 140 } }
    }
    Repeater {
        model: image.status === Image.Ready ? 4 : 0
        delegate: Shape {
            required property int index
            width: poster.radius
            height: poster.radius
            x: index % 2 === 0 ? 0 : poster.width - width
            y: index < 2 ? 0 : poster.height - height
            rotation: [0, 90, 270, 180][index]
            ShapePath {
                strokeWidth: -1
                fillColor: poster.cornerColor
                startX: 0; startY: 0
                PathLine { x: poster.radius; y: 0 }
                PathArc { x: 0; y: poster.radius; radiusX: poster.radius; radiusY: poster.radius; direction: PathArc.Counterclockwise }
                PathLine { x: 0; y: 0 }
            }
        }
    }
    // Border on top of the masks (hover/focus outline from the parent).
    Rectangle {
        anchors.fill: parent
        radius: poster.radius
        color: "transparent"
        border.width: poster.border.width
        border.color: poster.border.color
        visible: poster.border.width > 0
    }
}
