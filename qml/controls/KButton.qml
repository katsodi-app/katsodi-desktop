import QtQuick
import QtQuick.Controls
import Katsodi.App

// Katsodi button: primary (lime fill), secondary (outline) or ghost.
Button {
    id: control
    property string kind: "primary"
    property bool small: false
    implicitHeight: small ? 32 : 44
    leftPadding: small ? 15 : 18
    rightPadding: leftPadding
    font.pixelSize: 12
    font.weight: Font.Bold
    font.letterSpacing: 0.06
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.kind === "primary" ? Theme.c.primaryForeground : Theme.c.foreground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        opacity: control.enabled ? 1 : 0.5
    }
    background: Rectangle {
        radius: control.small ? 8 : 11
        color: control.kind === "primary"
               ? (control.down ? Qt.darker(Theme.c.primary, 1.08) : control.hovered ? Qt.lighter(Theme.c.primary, 1.04) : Theme.c.primary)
               : control.kind === "secondary"
                 ? (control.hovered ? Theme.c.surfaceHover : "transparent")
                 : (control.hovered ? Theme.c.surfaceElevated : "transparent")
        border.width: control.kind === "secondary" ? 1 : 0
        border.color: Theme.c.borderStrong
        opacity: control.enabled ? 1 : 0.5
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: parent.radius + 3
            color: "transparent"
            border.width: 2
            border.color: Theme.c.focusRing
            visible: control.visualFocus
        }
    }
}
