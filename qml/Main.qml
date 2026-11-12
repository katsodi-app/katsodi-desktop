import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "shell"
import "screens"

ApplicationWindow {
    id: window
    title: route.name === "room" && Room.name ? Room.name + " · Katsodi" : "Katsodi"
    minimumWidth: 320
    minimumHeight: 420
    visible: false // main.cpp restores geometry, then shows
    color: Theme.c.background
    font.family: Theme.fontFamily

    // Typed in-app destinations; no web router or URL is involved.
    property var route: ({ name: "home", id: "" })
    property var history: []
    readonly property bool compact: width < 860

    function navigate(name, id) {
        const next = { name: name, id: id || "" }
        if (route.name === next.name && route.id === next.id)
            return
        history.push(route)
        route = next
    }
    function back() {
        if (history.length)
            route = history.pop()
        else
            route = { name: "home", id: "" }
    }

    Shortcut { sequences: [StandardKey.Back, "Alt+Left"]; onActivated: window.back() }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.navigate("settings") }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Sidebar {
            Layout.fillHeight: true
            visible: !window.compact
            current: window.route.name
            onNavigate: (name) => window.navigate(name)
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            TopBar {
                Layout.fillWidth: true
                compact: window.compact
                onNavigate: (name) => window.navigate(name)
            }

            Loader {
                id: screen
                Layout.fillWidth: true
                Layout.fillHeight: true
                // Screens are created on demand and released when left.
                sourceComponent: {
                    switch (window.route.name) {
                    case "rooms": return roomsScreen
                    case "room": return roomScreen
                    case "fandoms": return fandomsScreen
                    case "settings": return settingsScreen
                    case "signin": return signInScreen
                    default: return homeScreen
                    }
                }
            }
        }
    }

    Component { id: homeScreen; HomeScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component { id: roomsScreen; RoomsScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component { id: fandomsScreen; FandomsScreen {} }
    Component { id: settingsScreen; SettingsScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component { id: signInScreen; SignInScreen { onDone: window.back() } }
    Component {
        id: roomScreen
        RoomScreen {
            roomId: window.route.id
            onBack: window.back()
            onNavigate: (name, id) => window.navigate(name, id)
        }
    }

    // Compact navigation for narrow windows, as on the website's phone layout.
    footer: Rectangle {
        visible: window.compact
        height: visible ? 60 : 0
        color: Theme.c.surface
        Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.c.border }
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 8
            anchors.rightMargin: 8
            Repeater {
                model: [
                    { name: "home", label: I18n.s.navHome },
                    { name: "rooms", label: I18n.s.navRooms },
                    { name: "fandoms", label: I18n.s.navFandoms },
                    { name: "settings", label: I18n.s.navSettings }
                ]
                delegate: ToolButton {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    text: modelData.label
                    font.pixelSize: 11
                    font.weight: Font.Medium
                    palette.buttonText: window.route.name === modelData.name ? Theme.c.foreground : Theme.c.mutedForeground
                    background: Rectangle { color: "transparent" }
                    onClicked: window.navigate(modelData.name)
                    Accessible.name: modelData.label
                }
            }
        }
    }
}
