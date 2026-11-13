import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Pill button (overview .ov-button): "primary" is the bright pill, "accent"
// the lime one, "ghost" glass, "quiet" text only. Presses sink slightly.
Button {
    id: control
    property string kind: "primary"
    property bool small: false
    property string iconName: ""
    implicitHeight: small ? 36 : 46
    leftPadding: small ? 16 : 22
    rightPadding: leftPadding
    font.family: Theme.fontFamily
    font.pixelSize: small ? 12 : 13
    font.weight: Font.Bold
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: text

    readonly property color fill: kind === "primary" ? Theme.c.foreground
                                 : kind === "accent" ? Theme.c.primary
                                 : kind === "ghost" ? (hovered ? K.glassHover : K.glass)
                                 : (hovered ? K.glass : "transparent")
    readonly property color ink: kind === "primary" ? Theme.c.background
                                : kind === "accent" ? Theme.c.primaryForeground
                                : Theme.c.foreground

    scale: down ? 0.96 : (hovered && enabled ? 1.02 : 1)
    Behavior on scale { NumberAnimation { duration: K.fast; easing.type: K.easing } }

    contentItem: RowLayout {
        spacing: 8
        opacity: control.enabled ? 1 : 0.45
        Icon {
            visible: control.iconName !== ""
            name: control.iconName
            size: control.small ? 15 : 17
            color: control.ink
            Layout.alignment: Qt.AlignVCenter
        }
        Text {
            text: control.text
            visible: text.length > 0
            font: control.font
            color: control.ink
            elide: Text.ElideRight
            Layout.alignment: Qt.AlignVCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
        }
    }
    background: Rectangle {
        radius: height / 2
        color: control.fill
        opacity: control.enabled ? 1 : 0.5
        border.width: control.kind === "ghost" ? 1 : 0
        border.color: control.hovered ? K.strokeStrong : K.stroke
        Behavior on color { ColorAnimation { duration: K.fast } }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -4
            radius: height / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.c.focusRing
            visible: control.visualFocus
        }
    }
}
