import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// Round icon-only button with a tooltip (Liner's control style).
AbstractButton {
    id: control
    property string iconName: "plus"
    property string tip: ""
    property bool active: false
    property int diameter: 40
    implicitWidth: diameter
    implicitHeight: diameter
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: tip
    Accessible.role: Accessible.Button

    ToolTip.visible: hovered && tip.length > 0
    ToolTip.delay: 450
    ToolTip.text: tip

    scale: down ? 0.92 : 1
    Behavior on scale { NumberAnimation { duration: K.fast; easing.type: K.easing } }

    background: Rectangle {
        radius: width / 2
        color: control.active ? Theme.c.primary : control.hovered ? K.glassHover : K.glass
        border.width: control.active ? 0 : 1
        border.color: control.hovered ? K.strokeStrong : K.stroke
        Behavior on color { ColorAnimation { duration: K.fast } }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -3
            radius: width / 2
            color: "transparent"
            border.width: 2
            border.color: Theme.c.focusRing
            visible: control.visualFocus
        }
    }
    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            name: control.iconName
            size: Math.round(control.diameter * 0.45)
            color: control.active ? Theme.c.primaryForeground : Theme.c.foreground
            opacity: control.enabled ? 1 : 0.4
        }
    }
}
