import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

ApplicationWindow {
    id: window
    title: route.name === "room" && Room.name ? Room.name + " · Katsodi" : "Katsodi"
    minimumWidth: 360
    minimumHeight: 480
    visible: false // main.cpp restores geometry, then shows
    // Our own title bar and resize edges (TitleBar, ResizeFrame).
    flags: Qt.Window | Qt.FramelessWindowHint
    color: Theme.c.background
    font.family: Theme.fontFamily

    // Typed in-app destinations; no web router or URL is involved.
    property var route: ({ name: "home", id: "" })
    property var history: []
    readonly property bool compact: width < 720

    function navigate(name, id) {
        const next = { name: name, id: id || "" }
        if (route.name === next.name && route.id === next.id)
            return
        history.push(route)
        if (history.length > 50)
            history.shift()
        route = next
    }
    // Development capture mode (main.cpp): a route, or an overlay to show.
    function captureStep(name, id) {
        search.close()
        create.close()
        if (name === "search") {
            Search.query = id
            search.open()
        } else if (name === "stage" || name === "dialog" || name === "panel") {
            // Development capture only: the room stage pane, or a demo dialog.
            if (screen.item && screen.item.captureDemo)
                screen.item.captureDemo(name, id)
        } else if (name === "create") {
            create.open()
        } else {
            navigate(name, id)
        }
    }
    function back() {
        route = history.length ? history.pop() : { name: "home", id: "" }
    }

    Shortcut { sequences: [StandardKey.Back, "Alt+Left"]; onActivated: window.back() }
    Shortcut { sequence: "Ctrl+,"; onActivated: window.navigate("settings") }
    Shortcut { sequences: ["Ctrl+K", StandardKey.Find]; onActivated: search.open() }
    Shortcut { sequence: "Ctrl+N"; onActivated: create.open() }

    // The room's colours tint the ambient glow while it is open.
    readonly property var roomTones: ({ orbit: [Theme.c.accent, Theme.c.info], wave: [Theme.c.info, Theme.c.success],
                                       moon: [Theme.c.warning, Theme.c.accent], sun: [Theme.c.danger, Theme.c.warning] })
    readonly property var tone: {
        const room = Room.roomId ? Rooms.room(Room.roomId) : null
        return room && room.art ? roomTones[room.art] : roomTones.orbit
    }
    Backdrop {
        anchors.fill: parent
        tintA: window.tone[0]
        tintB: window.tone[1]
        running: window.active && window.visibility !== Window.Minimized && window.visibility !== Window.Hidden
    }

    Rail {
        id: rail
        visible: !window.compact
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.margins: 14
        current: window.route.name
        onNavigate: (name) => window.navigate(name)
        onSearchRequested: search.open()
    }

    // Screens: created on demand, with a short rise-and-fade entrance.
    Loader {
        id: screen
        anchors.left: rail.visible ? rail.right : parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.topMargin: titleBar.height - 6
        anchors.leftMargin: rail.visible ? 6 : 0
        anchors.bottomMargin: (nowBar.shown ? nowBar.height + 24 : 0) + (window.compact ? 64 : 0)
        Behavior on anchors.bottomMargin { NumberAnimation { duration: K.base; easing.type: K.easing } }
        sourceComponent: {
            switch (window.route.name) {
            case "rooms": return roomsScreen
            case "room": return roomScreen
            case "fandoms": return fandomsScreen
            case "fandom": return fandomScreen
            case "friends": return friendsScreen
            case "music": return musicScreen
            case "profile": return profileScreen
            case "library": return libraryScreen
            case "settings": return settingsScreen
            case "signin": return signInScreen
            default: return homeScreen
            }
        }
        onLoaded: {
            if (K.still)
                return
            item.opacity = 0
            entrance.restart()
        }
    }
    ParallelAnimation {
        id: entrance
        NumberAnimation { target: screen.item; property: "opacity"; from: 0; to: 1; duration: K.base; easing.type: K.easing }
        NumberAnimation { target: screen.item; property: "y"; from: 14; to: 0; duration: K.slow; easing.type: K.easing }
    }

    Component { id: homeScreen; HomeScreen { onNavigate: (name, id) => window.navigate(name, id); onCreateRequested: create.open() } }
    Component { id: roomsScreen; RoomsScreen { onNavigate: (name, id) => window.navigate(name, id); onCreateRequested: create.open() } }
    Component { id: fandomsScreen; FandomsScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component { id: musicScreen; MusicScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component { id: friendsScreen; FriendsScreen { onNavigate: (name, id) => window.navigate(name, id) } }
    Component {
        id: profileScreen
        ProfileScreen {
            username: window.route.id
            onBack: window.back()
            onNavigate: (name, id) => window.navigate(name, id)
        }
    }
    Component {
        id: fandomScreen
        FandomScreen {
            fandomId: window.route.id
            onBack: window.back()
            onNavigate: (name, id) => window.navigate(name, id)
            onCreateRequested: create.open()
        }
    }
    Component { id: libraryScreen; LibraryScreen { onNavigate: (name, id) => window.navigate(name, id); onCreateRequested: create.open() } }
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

    TitleBar {
        id: titleBar
        window: window
        anchors.left: rail.visible ? rail.right : parent.left
        anchors.right: parent.right
        anchors.top: parent.top
    }
    // A thin outline, since a frameless window has no system border.
    Rectangle {
        anchors.fill: parent
        visible: window.visibility === Window.Windowed
        color: "transparent"
        border.width: 1
        border.color: K.strokeStrong
        z: 999
    }
    ResizeFrame { window: window }

    NowBar {
        id: nowBar
        shown: Room.roomId !== "" && window.route.name !== "room"
        anchors.left: screen.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 14
        anchors.rightMargin: 14
        anchors.bottomMargin: 14 + (window.compact ? 64 : 0)
        onReturnRequested: window.navigate("room", Room.roomId)
    }

    // Compact navigation for narrow windows.
    Glass {
        visible: window.compact
        strong: true
        radius: 24
        height: 58
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 8
        RowLayout {
            anchors.fill: parent
            anchors.margins: 6
            Repeater {
                model: [{ name: "home", icon: "home", label: I18n.s.navHome },
                        { name: "rooms", icon: "rooms", label: I18n.s.navRooms },
                        { name: "fandoms", icon: "fandoms", label: I18n.s.navFandoms },
                        { name: "music", icon: "music", label: I18n.s.navMusic },
                        { name: "library", icon: "library", label: I18n.s.navLibrary },
                        { name: "friends", icon: "friends", label: I18n.s.navFriends },
                        { name: "settings", icon: "settings", label: I18n.s.navSettings }]
                delegate: Item {
                    required property var modelData
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    IconButton {
                        anchors.centerIn: parent
                        iconName: modelData.icon
                        tip: modelData.label
                        active: window.route.name === modelData.name || (modelData.name === "rooms" && window.route.name === "room")
                        onClicked: window.navigate(modelData.name)
                    }
                }
            }
        }
    }

    // The provider player is a separate native window: it hides while a
    // popup covers the stage, since nothing can be drawn over it.
    Binding { target: Player; property: "obscured"; value: search.visible || create.visible }
    Binding { target: Browser; property: "obscured"; value: search.visible || create.visible }
    Binding { target: Demo; property: "remoteAudioMuted"; value: true; when: Voice.deafened }
    Connections {
        target: Player
        function onExternalRequested(url) { Links.openExternal(url) }
    }

    SearchOverlay {
        id: search
        parent: Overlay.overlay
        onOpenRoom: (id) => window.navigate("room", id)
        onOpenFandom: (id) => window.navigate("fandom", id)
    }
    CreateRoomDialog {
        id: create
        parent: Overlay.overlay
        onCreatedShared: (roomId) => window.navigate("room", roomId)
        onCreatedDraft: window.navigate("library")
    }
}
