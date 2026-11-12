import QtQuick
import QtQuick.Controls
import Katsodi.App

// Filter chip: accent tint and outline when selected (website .chip.active).
Button {
    id: control
    property bool selected: false
    implicitHeight: 36
    leftPadding: 16
    rightPadding: 16
    font.pixelSize: 13
    font.weight: Font.DemiBold
    hoverEnabled: true
    checkable: false
    Accessible.role: Accessible.RadioButton
    Accessible.checked: selected
    Accessible.name: text

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.selected ? Theme.c.foreground : Theme.c.mutedForeground
        verticalAlignment: Text.AlignVCenter
        horizontalAlignment: Text.AlignHCenter
    }
    background: Rectangle {
        radius: 10
        color: control.selected ? Qt.alpha(Theme.c.accent, 0.14)
                                : control.hovered ? Theme.c.surfaceElevated : Theme.c.surface
        border.width: 1
        border.color: control.selected ? Qt.alpha(Theme.c.accent, 0.55) : Theme.c.border
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: 13
            color: "transparent"
            border.width: 2
            border.color: Theme.c.focusRing
            visible: control.visualFocus
        }
    }
}
