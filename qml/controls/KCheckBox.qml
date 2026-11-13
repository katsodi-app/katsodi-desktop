import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// A check box in the Katsodi palette, with a wrapping label.
CheckBox {
    id: control
    hoverEnabled: true
    font.family: Theme.fontFamily
    font.pixelSize: 14
    font.weight: Font.DemiBold
    indicator: Rectangle {
        implicitWidth: 24
        implicitHeight: 24
        x: control.leftPadding
        y: (control.height - height) / 2
        radius: 7
        color: control.checked ? Theme.c.primary : "transparent"
        border.width: 2
        border.color: control.checked ? Theme.c.primary : (control.hovered ? K.text : K.strokeStrong)
        Behavior on color { ColorAnimation { duration: K.fast } }
        Icon {
            anchors.centerIn: parent
            visible: control.checked
            name: "check"
            size: 16
            stroke: 2.4
            color: Theme.c.primaryForeground
        }
    }
    contentItem: Text {
        leftPadding: control.indicator.width + 12
        text: control.text
        font: control.font
        color: K.text
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.WordWrap
    }
}
