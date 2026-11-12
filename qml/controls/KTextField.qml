import QtQuick
import QtQuick.Controls
import Katsodi.App

TextField {
    id: control
    implicitHeight: 44
    leftPadding: 14
    rightPadding: 14
    font.pixelSize: 14
    color: Theme.c.foreground
    placeholderTextColor: Theme.c.mutedForeground
    selectionColor: Qt.alpha(Theme.c.accent, 0.35)
    selectedTextColor: Theme.c.foreground
    Accessible.name: placeholderText
    background: Rectangle {
        radius: 10
        color: Theme.c.surfaceElevated
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? Theme.c.focusRing : Theme.c.border
    }
}
