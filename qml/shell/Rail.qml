import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Liner-style icon rail in frosted glass: logo, sections, search, then
// settings and the account at the bottom. The lime marker glides to the
// current section.
Glass {
    id: rail
    property string current: "home"
    signal navigate(string name)
    signal searchRequested()
    implicitWidth: 64
    radius: 28
    strong: true

    readonly property var items: [
        { name: "home", icon: "home", label: I18n.s.navHome },
        { name: "rooms", icon: "rooms", label: I18n.s.navRooms },
        { name: "fandoms", icon: "fandoms", label: I18n.s.navFandoms },
        { name: "library", icon: "library", label: I18n.s.navLibrary },
        { name: "friends", icon: "friends", label: I18n.s.navFriends }
    ]
    readonly property string section: current === "room" ? "rooms" : current === "fandom" ? "fandoms" : current === "profile" ? "friends"
                                     : current === "signin" ? "settings" : current

    function itemFor(name) {
        if (name === "settings")
            return settingsButton
        for (let i = 0; i < nav.count; ++i)
            if (rail.items[i].name === name)
                return nav.itemAt(i)
        return null
    }
    readonly property Item activeItem: itemFor(section)

    // Gliding marker behind the active button.
    Rectangle {
        id: marker
        visible: rail.activeItem !== null
        width: 48; height: 48; radius: 24
        x: (rail.width - width) / 2
        // Layout positions are properties, so the marker follows reactively.
        y: rail.activeItem ? column.y + rail.activeItem.y + (rail.activeItem.height - height) / 2 : 0
        color: Theme.c.primary
        Behavior on y { NumberAnimation { duration: K.base; easing.type: K.easing } }
    }

    ColumnLayout {
        id: column
        anchors.fill: parent
        anchors.topMargin: 16
        anchors.bottomMargin: 16
        spacing: 8

        AbstractButton {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 14
            implicitWidth: 44; implicitHeight: 44
            hoverEnabled: true
            onClicked: rail.navigate("home")
            Accessible.name: "Katsodi"
            contentItem: Logo { size: 40; scale: parent.hovered ? 1.06 : 1; Behavior on scale { NumberAnimation { duration: K.fast } } }
            background: Item {}
        }

        Repeater {
            id: nav
            model: rail.items
            delegate: RailButton {
                required property var modelData
                Layout.alignment: Qt.AlignHCenter
                iconName: modelData.icon
                tip: modelData.label
                on: rail.section === modelData.name
                onClicked: rail.navigate(modelData.name)
            }
        }
        RailButton {
            Layout.alignment: Qt.AlignHCenter
            iconName: "search"
            tip: I18n.s.navSearch + "  (Ctrl K)"
            onClicked: rail.searchRequested()
        }

        Item { Layout.fillHeight: true }

        RailButton {
            id: settingsButton
            Layout.alignment: Qt.AlignHCenter
            iconName: "settings"
            tip: I18n.s.navSettings
            on: rail.section === "settings"
            onClicked: rail.navigate("settings")
        }
        AbstractButton {
            id: account
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: 6
            implicitWidth: 44; implicitHeight: 44
            hoverEnabled: true
            focusPolicy: Qt.StrongFocus
            Accessible.name: Account.signedIn ? Account.displayName : I18n.s.signIn
            ToolTip.visible: hovered
            ToolTip.delay: 450
            ToolTip.text: Account.signedIn ? Account.displayName : I18n.s.signIn
            onClicked: rail.navigate(Account.signedIn ? "settings" : "signin")
            background: Rectangle {
                radius: 22
                color: Theme.c.selected
                border.width: account.visualFocus ? 2 : (account.hovered ? 1 : 0)
                border.color: account.visualFocus ? Theme.c.focusRing : K.strokeStrong
            }
            contentItem: Text {
                text: Account.signedIn && Account.displayName ? Account.displayName.charAt(0).toUpperCase() : "K"
                color: Theme.c.accent
                font.family: Theme.displayFamily
                font.pixelSize: 18
                font.weight: Font.Bold
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
            }
            // Connection dot.
            Rectangle {
                width: 10; height: 10; radius: 5
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                border.width: 2
                border.color: K.glassStrong
                color: Account.server === "online" ? Theme.c.success
                       : Account.server === "checking" ? Theme.c.mutedForeground : Theme.c.warning
            }
        }
    }

    component RailButton: AbstractButton {
        id: button
        property string iconName
        property string tip
        property bool on: false
        implicitWidth: 48
        implicitHeight: 48
        hoverEnabled: true
        focusPolicy: Qt.StrongFocus
        Accessible.name: tip
        Accessible.role: Accessible.Button
        ToolTip.visible: hovered
        ToolTip.delay: 450
        ToolTip.text: tip
        scale: down ? 0.9 : 1
        Behavior on scale { NumberAnimation { duration: K.fast; easing.type: K.easing } }
        background: Rectangle {
            radius: 24
            color: button.hovered && !button.on ? K.glassHover : "transparent"
            border.width: button.visualFocus ? 2 : 0
            border.color: Theme.c.focusRing
            Behavior on color { ColorAnimation { duration: K.fast } }
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: button.iconName
                size: 21
                color: button.on ? Theme.c.primaryForeground : (button.hovered ? Theme.c.foreground : Theme.c.mutedForeground)
                Behavior on color { ColorAnimation { duration: K.fast } }
            }
        }
    }
}
