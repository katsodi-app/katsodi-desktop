import QtQuick
import Katsodi.App
import Katsodi.Ui

// Frosted panel (overview .ov-glass): translucent fill, hairline border and
// a soft top highlight. No blur layer: the backdrop is already blurred.
Rectangle {
    id: glass
    property bool strong: false
    /** Nearly opaque, for floating panels over other content. */
    property bool solid: false
    property bool hoverable: false
    property bool hovered: false
    radius: K.rCard
    color: solid ? Qt.alpha(Theme.c.surface, 0.97) : strong ? K.glassStrong : (hoverable && hovered ? K.glassHover : K.glass)
    border.width: 1
    border.color: hoverable && hovered ? K.strokeStrong : K.stroke
    Behavior on color { ColorAnimation { duration: K.fast } }
    Behavior on border.color { ColorAnimation { duration: K.fast } }

    Rectangle {
        // inset 0 1px highlight
        anchors.top: parent.top
        anchors.topMargin: 1
        anchors.horizontalCenter: parent.horizontalCenter
        width: parent.width - 2 * parent.radius
        height: 1
        color: K.highlight
        opacity: 0.6
    }
}
