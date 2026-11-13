import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// A small yes/no question before a change to room access (the website asks
// the same with a browser confirm). ask() fills it and opens it.
Popup {
    id: dialog
    property string title
    property string body
    property bool danger: false
    property var action: null
    function ask(titleText, bodyText, isDanger, onConfirm) {
        title = titleText
        body = bodyText
        danger = isDanger
        action = onConfirm
        open()
    }
    parent: Overlay.overlay
    modal: true
    focus: true
    padding: 24
    x: Math.round((parent.width - width) / 2)
    y: Math.round(Math.max(24, (parent.height - height) / 2))
    width: Math.min(460, parent.width - 48)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: action = null

    Overlay.modal: Rectangle { color: Qt.alpha(Theme.c.background, 0.8) }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base } }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }
    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 14
        Text {
            Layout.fillWidth: true
            text: dialog.title
            color: K.text
            font.family: Theme.displayFamily
            font.pixelSize: 22
            font.weight: Font.Bold
            wrapMode: Text.WordWrap
        }
        Text {
            Layout.fillWidth: true
            text: dialog.body
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 6
            spacing: 10
            Item { Layout.fillWidth: true }
            KButton { kind: "ghost"; text: I18n.s.modCancel; onClicked: dialog.close() }
            KButton {
                id: confirm
                kind: dialog.danger ? "accent" : "primary"
                text: I18n.s.modConfirm
                onClicked: {
                    const run = dialog.action
                    dialog.close()
                    if (run) run()
                }
            }
        }
    }
    onOpened: confirm.forceActiveFocus()
}
