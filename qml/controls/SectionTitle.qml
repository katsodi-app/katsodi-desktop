import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Liner-style section heading: a small icon, a bold title and a quiet link.
RowLayout {
    id: section
    property string iconName: "flame"
    property string title
    property string action
    signal activated()
    spacing: 10

    Icon {
        name: section.iconName
        size: 18
        color: Theme.c.accent
        Layout.alignment: Qt.AlignVCenter
    }
    Text {
        Layout.fillWidth: true
        text: section.title
        color: K.text
        font.family: Theme.fontFamily
        font.pixelSize: 20
        font.weight: Font.Bold
        font.letterSpacing: -0.6
        elide: Text.ElideRight
        Accessible.role: Accessible.Heading
    }
    AbstractButton {
        id: link
        visible: section.action.length > 0
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: section.action
        onClicked: section.activated()
        implicitHeight: 32
        implicitWidth: linkRow.implicitWidth + 16
        background: Rectangle {
            radius: 16
            color: link.hovered ? K.glass : "transparent"
            border.width: link.visualFocus ? 2 : 0
            border.color: Theme.c.focusRing
        }
        contentItem: Item {
            Row {
                id: linkRow
                anchors.centerIn: parent
                spacing: 4
                Text {
                    text: section.action
                    color: link.hovered ? K.text : K.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    anchors.verticalCenter: parent.verticalCenter
                }
                Icon {
                    name: "chevron"
                    size: 14
                    color: link.hovered ? K.text : K.muted
                    anchors.verticalCenter: parent.verticalCenter
                    x: link.hovered ? 2 : 0
                    Behavior on x { NumberAnimation { duration: K.fast } }
                }
            }
        }
    }
}
