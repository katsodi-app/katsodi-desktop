import QtQuick
import Katsodi.App
import Katsodi.Ui

// Artwork tile. The image comes from the C++ poster provider, decoded at
// this tile's pixel size with rounded corners baked in (no extra layers).
// Size requests are rounded to 24 px steps so resizing does not re-decode
// on every pixel. Falls back to initials on the selected tint.
Item {
    id: poster
    property string art: ""          // "fandoms/x.webp"
    property string initials: "K"
    property int radius: 16
    property bool framed: false      // thin outline and lift when inset on art
    readonly property real dpr: Screen.devicePixelRatio
    readonly property int reqW: Math.max(24, Math.ceil(width * dpr / 24) * 24)
    readonly property int reqH: Math.max(24, Math.ceil(height * dpr / 24) * 24)

    Rectangle {
        anchors.fill: parent
        radius: poster.radius
        color: Theme.c.selected
        visible: image.status !== Image.Ready
        Text {
            anchors.centerIn: parent
            text: poster.initials
            color: Theme.c.accent
            font.family: Theme.displayFamily
            font.pixelSize: Math.min(poster.width, poster.height) * 0.3
            font.weight: Font.Bold
        }
    }
    Image {
        id: image
        anchors.fill: parent
        asynchronous: true
        cache: true
        smooth: true
        antialiasing: poster.framed   // edges stay clean when the card tilts it
        fillMode: Image.Stretch
        source: poster.art !== "" && poster.visible && poster.width > 0
                ? "image://poster/" + poster.reqW + "x" + poster.reqH + "x" + Math.round(poster.radius * poster.dpr) + "/" + poster.art
                : ""
        opacity: status === Image.Ready ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: K.base } }
    }
    // Frame drawn over the image edge: a hairline plus a darker outer ring,
    // so an inset poster separates from busy art without a shadow layer.
    Rectangle {
        visible: poster.framed
        anchors.fill: parent
        anchors.margins: -2
        radius: poster.radius + 2
        color: "transparent"
        border.width: 2
        border.color: Qt.alpha(Theme.c.background, 0.55)
    }
    Rectangle {
        visible: poster.framed
        anchors.fill: parent
        radius: poster.radius
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(1, 1, 1, 0.28)
    }
}
