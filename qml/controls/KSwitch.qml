import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// A switch in the Katsodi palette: a pill track that fills with the primary
// colour, a knob that slides.
Switch {
    id: control
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: 14
    font.weight: Font.DemiBold
    indicator: Rectangle {
        implicitWidth: 44
        implicitHeight: 26
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 13
        color: control.checked ? Theme.c.primary : K.glassHover
        border.width: 1
        border.color: control.checked ? Theme.c.primary : (control.hovered ? K.strokeStrong : K.stroke)
        opacity: control.enabled ? 1 : 0.5
        Behavior on color { ColorAnimation { duration: K.fast } }
        Rectangle {
            width: 20; height: 20; radius: 10
            y: 3
            x: control.checked ? parent.width - width - 3 : 3
            color: control.checked ? Theme.c.primaryForeground : Theme.c.foreground
            Behavior on x { NumberAnimation { duration: K.fast; easing.type: K.easing } }
        }
    }
    contentItem: Text {
        leftPadding: control.indicator.width + (control.text ? 12 : 0)
        text: control.text
        font: control.font
        color: K.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
