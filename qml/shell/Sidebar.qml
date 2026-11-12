import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App

// The website's sidebar: 218 px, brand, tagline, "Explore" navigation and
// the account at the bottom.
Rectangle {
    id: sidebar
    property string current: "home"
    signal navigate(string name)
    implicitWidth: 218
    color: Theme.c.background

    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.c.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 34
        anchors.leftMargin: 20
        anchors.rightMargin: 20
        anchors.bottomMargin: 22
        spacing: 0

        // Brand: the logo mark and "katsodi." with the accent dot.
        RowLayout {
            spacing: 6
            Item {
                implicitWidth: 39
                implicitHeight: 41
                Rectangle {
                    anchors.centerIn: parent
                    width: 34; height: 34; radius: 10
                    rotation: -8
                    color: Theme.c.primary
                }
                Repeater {
                    // Bars of the logo (src/app/icon.svg), in 64-unit space.
                    model: [[16, 28, 36], [24, 22, 42], [32, 16, 48], [40, 26, 38], [48, 22, 42]]
                    delegate: Rectangle {
                        required property var modelData
                        readonly property real s: 39 / 64
                        x: modelData[0] * s - width / 2
                        y: modelData[1] * s + 1
                        width: 2
                        radius: 1
                        height: (modelData[2] - modelData[1]) * s
                        color: Theme.c.primaryForeground
                    }
                }
            }
            Text {
                textFormat: Text.StyledText
                text: "katsodi<font color='" + Theme.c.accent + "'>.</font>"
                color: Theme.c.foreground
                font.pixelSize: 30
                font.weight: Font.Bold
                font.letterSpacing: -1.6
            }
        }
        Text {
            Layout.leftMargin: 43
            Layout.topMargin: 4
            text: I18n.s.tagline
            color: Theme.c.mutedForeground
            font.pixelSize: 11
        }

        Text {
            Layout.topMargin: 44
            Layout.leftMargin: 16
            Layout.bottomMargin: 12
            text: I18n.s.navExplore.toUpperCase()
            color: Theme.c.mutedForeground
            font.pixelSize: 11
            font.weight: Font.Bold
            font.letterSpacing: 1.54
        }

        NavItem { label: I18n.s.navHome; glyph: "home"; active: sidebar.current === "home"; onClicked: sidebar.navigate("home") }
        NavItem { label: I18n.s.navRooms; glyph: "rooms"; active: sidebar.current === "rooms" || sidebar.current === "room"; onClicked: sidebar.navigate("rooms") }
        NavItem { label: I18n.s.navFandoms; glyph: "fandoms"; active: sidebar.current === "fandoms"; onClicked: sidebar.navigate("fandoms") }

        Item { Layout.fillHeight: true }

        NavItem { label: I18n.s.navSettings; glyph: "settings"; active: sidebar.current === "settings"; onClicked: sidebar.navigate("settings") }

        // Account
        ItemDelegate {
            id: account
            Layout.fillWidth: true
            Layout.topMargin: 10
            implicitHeight: 52
            hoverEnabled: true
            onClicked: sidebar.navigate(Account.signedIn ? "settings" : "signin")
            Accessible.name: Account.signedIn ? Account.displayName : I18n.s.signIn
            background: Rectangle { radius: 12; color: account.hovered ? Theme.c.surfaceElevated : "transparent" }
            contentItem: RowLayout {
                spacing: 10
                Rectangle {
                    implicitWidth: 32; implicitHeight: 32; radius: 16
                    color: Theme.c.selected
                    Text {
                        anchors.centerIn: parent
                        text: Account.signedIn && Account.displayName ? Account.displayName.charAt(0).toUpperCase() : "K"
                        color: Theme.c.accent
                        font.weight: Font.Bold
                        font.pixelSize: 13
                    }
                }
                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Text {
                        Layout.fillWidth: true
                        text: Account.signedIn ? Account.displayName : I18n.s.signIn
                        color: Theme.c.foreground
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                    Text {
                        Layout.fillWidth: true
                        text: Account.server === "online" ? I18n.s.serverOnline
                              : Account.server === "checking" ? I18n.s.loading
                              : Account.server === "unconfigured" ? I18n.s.serverUnconfigured
                              : I18n.s.errorNetwork
                        color: Account.server === "online" ? Theme.c.success : Theme.c.mutedForeground
                        font.pixelSize: 10
                        elide: Text.ElideRight
                    }
                }
            }
        }
    }
}
