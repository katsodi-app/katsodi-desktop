import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Top bar: connection state, the RU/EN switch and the account action, as in
// the website's .topbar (88 px, 44 px side padding).
Item {
    id: bar
    property bool compact: false
    signal navigate(string name)
    implicitHeight: compact ? 64 : 88

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: bar.compact ? 16 : 44
        anchors.rightMargin: bar.compact ? 16 : 44
        spacing: 14

        // Connection notice (website "Сервер подключён" strip).
        Rectangle {
            visible: !bar.compact
            implicitHeight: 32
            implicitWidth: status.implicitWidth + 30
            radius: 8
            color: Theme.c.surface
            border.width: 1
            border.color: Theme.c.border
            RowLayout {
                id: status
                anchors.centerIn: parent
                spacing: 8
                Rectangle {
                    width: 6; height: 6; radius: 3
                    color: Account.server === "online" ? Theme.c.success
                           : Account.server === "checking" ? Theme.c.mutedForeground : Theme.c.warning
                }
                Text {
                    text: Account.server === "online" ? I18n.s.serverOnline
                          : Account.server === "checking" ? I18n.s.loading
                          : Account.server === "unconfigured" ? I18n.s.serverUnconfigured
                          : I18n.s.serverOffline
                    color: Theme.c.mutedForeground
                    font.pixelSize: 11
                }
            }
            MouseArea {
                anchors.fill: parent
                enabled: Account.server === "offline"
                cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
                onClicked: Account.retryServer()
            }
        }

        Item { Layout.fillWidth: true }

        // RU / EN, website .language-switch
        Rectangle {
            implicitWidth: 76
            implicitHeight: 43
            radius: 9
            color: Theme.c.surface
            border.width: 1
            border.color: Theme.c.border
            Row {
                anchors.centerIn: parent
                spacing: 2
                Repeater {
                    model: ["ru", "en"]
                    delegate: Button {
                        id: lang
                        required property string modelData
                        width: 34; height: 36
                        text: modelData.toUpperCase()
                        font.pixelSize: 12
                        hoverEnabled: true
                        Accessible.name: modelData === "ru" ? "Русский" : "English"
                        onClicked: Prefs.locale = modelData
                        contentItem: Text {
                            text: lang.text
                            font: lang.font
                            color: Prefs.locale === lang.modelData ? Theme.c.primary : Theme.c.mutedForeground
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            radius: 6
                            color: Prefs.locale === lang.modelData ? Theme.c.selected : lang.hovered ? Theme.c.surfaceElevated : "transparent"
                            border.width: lang.visualFocus ? 2 : 0
                            border.color: Theme.c.focusRing
                        }
                    }
                }
            }
        }

        KButton {
            small: true
            visible: !Account.signedIn
            text: I18n.s.signIn
            onClicked: bar.navigate("signin")
        }
    }
}
