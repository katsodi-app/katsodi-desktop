import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Shown every time before a shared browser opens: what it means that it
// runs on this computer, who should get access, and the consequences. The
// start button stays off until the host ticks that they understand.
Popup {
    id: dialog
    property string url
    signal confirmed(string url)
    parent: Overlay.overlay
    modal: true
    focus: true
    padding: 0
    x: Math.round((parent.width - width) / 2)
    y: Math.round(Math.max(24, (parent.height - height) / 2))
    width: Math.min(640, parent.width - 48)
    closePolicy: Popup.CloseOnEscape
    onOpened: agree.checked = false

    Overlay.modal: Rectangle { color: Qt.alpha(Theme.c.background, 0.84) }
    enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base } }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }
    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 16
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.bottomMargin: 0
            spacing: 14
            Rectangle {
                implicitWidth: 48; implicitHeight: 48; radius: 16
                color: Qt.alpha(Theme.c.warning, 0.18)
                Icon { anchors.centerIn: parent; name: "globe"; size: 24; color: Theme.c.warning }
            }
            Text {
                Layout.fillWidth: true
                text: I18n.s.browserDisclaimerTitle
                color: K.text
                font.family: Theme.displayFamily
                font.pixelSize: 26
                font.weight: Font.Bold
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 12
            Repeater {
                model: [I18n.s.browserDisclaimer1, I18n.s.browserDisclaimer2, I18n.s.browserDisclaimer3,
                        I18n.s.browserDisclaimer4, I18n.s.browserDisclaimer5]
                delegate: RowLayout {
                    required property string modelData
                    required property int index
                    Layout.fillWidth: true
                    spacing: 12
                    Rectangle {
                        Layout.alignment: Qt.AlignTop
                        Layout.topMargin: 6
                        implicitWidth: 8; implicitHeight: 8; radius: 4
                        // The three about other people's access carry the warning colour.
                        color: index < 4 ? Theme.c.warning : Theme.c.success
                    }
                    Text {
                        Layout.fillWidth: true
                        text: modelData
                        color: K.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                        lineHeight: 1.2
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        KCheckBox {
            id: agree
            Layout.leftMargin: 18
            Layout.rightMargin: 24
            Layout.fillWidth: true
            text: I18n.s.browserDisclaimerAgree
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.topMargin: 0
            spacing: 10
            Item { Layout.fillWidth: true }
            KButton {
                kind: "ghost"
                text: I18n.s.cancel
                onClicked: dialog.close()
            }
            KButton {
                kind: "primary"
                iconName: "globe"
                text: I18n.s.browserDisclaimerStart
                enabled: agree.checked
                onClicked: {
                    dialog.confirmed(dialog.url)
                    dialog.close()
                }
            }
        }
    }
}
