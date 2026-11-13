import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// Pill segmented control with a highlight that glides to the selection
// (overview .ov-pills). model: [{ value, label }].
Rectangle {
    id: root
    property var model: []
    property string current: ""
    signal selected(string value)
    implicitHeight: 42
    implicitWidth: row.implicitWidth + 8
    radius: height / 2
    color: K.glass
    border.width: 1
    border.color: K.stroke
    Accessible.role: Accessible.PageTabList

    readonly property Item currentItem: {
        for (let i = 0; i < repeater.count; ++i)
            if (root.model[i] && root.model[i].value === root.current)
                return repeater.itemAt(i)
        return null
    }

    Rectangle {
        id: highlight
        y: 4
        height: root.height - 8
        radius: height / 2
        color: Theme.c.foreground
        visible: root.currentItem !== null
        x: root.currentItem ? root.currentItem.x + row.x : 4
        width: root.currentItem ? root.currentItem.width : 0
        Behavior on x { NumberAnimation { duration: K.base; easing.type: K.easing } }
        Behavior on width { NumberAnimation { duration: K.base; easing.type: K.easing } }
    }

    Row {
        id: row
        x: 4
        anchors.verticalCenter: parent.verticalCenter
        Repeater {
            id: repeater
            model: root.model
            delegate: AbstractButton {
                id: option
                required property var modelData
                readonly property bool on: modelData.value === root.current
                height: root.height - 8
                width: label.implicitWidth + 32
                hoverEnabled: true
                focusPolicy: Qt.StrongFocus
                Accessible.role: Accessible.PageTab
                Accessible.name: modelData.label
                Accessible.selected: on
                onClicked: root.selected(modelData.value)
                contentItem: Text {
                    id: label
                    text: option.modelData.label
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: option.on ? Theme.c.background : (option.hovered ? Theme.c.foreground : Theme.c.mutedForeground)
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    Behavior on color { ColorAnimation { duration: K.fast } }
                }
                background: Rectangle {
                    radius: height / 2
                    color: "transparent"
                    border.width: option.visualFocus ? 2 : 0
                    border.color: Theme.c.focusRing
                }
            }
        }
    }
}
