import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// The presenter's list of who may control the shared browser: everyone, or
// chosen members. Nobody has control until the presenter gives it, and the
// reminder stays in view.
Popup {
    id: panel
    parent: Overlay.overlay
    modal: true
    focus: true
    padding: 0
    x: Math.round((parent.width - width) / 2)
    y: Math.round(Math.max(24, (parent.height - height) / 2))
    width: Math.min(460, parent.width - 48)
    height: Math.min(560, parent.height - 48)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    // Everyone but yourself (the presenter).
    readonly property var others: Room.members.filter(m => m.id.toLowerCase() !== Room.viewerId
                                                       && m.id.toLowerCase() !== Room.demoUserId)

    Overlay.modal: Rectangle { color: Qt.alpha(Theme.c.background, 0.8) }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base } }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }
    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 12
        Text {
            Layout.fillWidth: true
            Layout.margins: 22
            Layout.bottomMargin: 0
            text: I18n.s.browserAccessTitle
            color: K.text
            font.family: Theme.displayFamily
            font.pixelSize: 24
            font.weight: Font.Bold
        }
        Rectangle {
            Layout.fillWidth: true
            Layout.leftMargin: 22
            Layout.rightMargin: 22
            implicitHeight: hint.implicitHeight + 20
            radius: 12
            color: Qt.alpha(Theme.c.warning, 0.14)
            Text {
                id: hint
                anchors.fill: parent
                anchors.margins: 10
                text: I18n.s.browserAccessHint
                color: K.text
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        KSwitch {
            Layout.leftMargin: 18
            text: I18n.s.browserAccessAll
            checked: Demo.controlAll
            onToggled: Demo.setControlAll(checked)
        }
        Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: K.stroke }
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 8
            Layout.rightMargin: 8
            Layout.bottomMargin: 12
            clip: true
            model: panel.others
            spacing: 2
            ScrollBar.vertical: ScrollBar {}
            delegate: Rectangle {
                id: row
                required property var modelData
                readonly property bool allowed: Demo.controlAll || Demo.controllers.indexOf(modelData.id.toLowerCase()) >= 0
                width: ListView.view.width
                height: 52
                radius: 12
                color: rowHover.hovered ? K.glassHover : "transparent"
                HoverHandler { id: rowHover }
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 8
                    spacing: 12
                    Rectangle {
                        implicitWidth: 34; implicitHeight: 34; radius: 17
                        color: Theme.c.selected
                        Text { anchors.centerIn: parent; text: row.modelData.initials || "K"; color: Theme.c.accent
                               font.family: Theme.displayFamily; font.pixelSize: 15; font.weight: Font.Bold }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        color: K.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    KSwitch {
                        checked: row.allowed
                        enabled: !Demo.controlAll
                        onToggled: Demo.setControl(row.modelData.id, checked)
                        Accessible.name: row.modelData.name
                    }
                }
            }
            Text {
                anchors.centerIn: parent
                visible: parent.count === 0
                text: I18n.s.browserAccessNobody
                color: K.muted
                font.family: Theme.fontFamily
            }
        }
    }
}
