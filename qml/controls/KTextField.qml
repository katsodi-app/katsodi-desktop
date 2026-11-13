import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// Glass pill input with an optional leading icon.
TextField {
    id: control
    property string iconName: ""
    implicitHeight: 46
    leftPadding: iconName ? 44 : 18
    rightPadding: 18
    font.family: Theme.fontFamily
    font.pixelSize: 14
    color: Theme.c.foreground
    placeholderTextColor: Theme.c.mutedForeground
    selectionColor: Qt.alpha(Theme.c.accent, 0.35)
    selectedTextColor: Theme.c.foreground
    Accessible.name: placeholderText
    background: Rectangle {
        radius: height / 2
        color: control.activeFocus ? K.glassHover : K.glass
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.c.focusRing : K.stroke
        Behavior on color { ColorAnimation { duration: K.fast } }
        Icon {
            visible: control.iconName !== ""
            name: control.iconName
            size: 18
            color: Theme.c.mutedForeground
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
