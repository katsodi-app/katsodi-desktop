import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// The room's provider player. The video itself is drawn by the separate
// katsodi-player helper as a child window exactly over `surface`; this item
// reserves the space, shows state while the helper starts, and carries the
// native controls (play/pause, timeline for hosts, volume).
ColumnLayout {
    id: box
    spacing: 10
    property real maxVideoHeight: 560
    property bool music: Room.format === "music" || Room.syncKind === "soundcloud"

    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const h = Math.floor(s / 3600), m = Math.floor(s % 3600 / 60), r = s % 60
        return (h > 0 ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(r).padStart(2, "0")
    }

    // Reports where the helper should draw: on every frame the window
    // renders (nothing renders while idle, so this costs nothing at rest).
    function report() {
        const window = surface.Window.window
        if (!window)
            return
        const p = surface.mapToItem(null, 0, 0)
        // Keep the island inside the window's content.
        const x = Math.max(0, p.x), y = Math.max(0, p.y)
        const w = Math.min(p.x + surface.width, window.width) - x
        const h = Math.min(p.y + surface.height, window.height) - y
        Player.setSurface(window, x, y, Math.max(0, w), Math.max(0, h), box.visible && w > 40 && h > 40)
    }
    Connections {
        target: surface.Window.window
        function onAfterAnimating() { box.report() }
    }
    onVisibleChanged: report()
    Component.onCompleted: Qt.callLater(report)
    Component.onDestruction: {
        const window = surface.Window.window
        if (window)
            Player.setSurface(window, 0, 0, 0, 0, false)
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: box.music ? 166 : Math.round(width * 9 / 16)
        Layout.maximumHeight: box.music ? 166 : box.maxVideoHeight
        radius: 14
        color: "#000000"
        border.width: 1
        border.color: K.stroke
        clip: true

        Item {
            id: surface
            anchors.fill: parent
            anchors.margins: 1
            onWidthChanged: box.report()
            onHeightChanged: box.report()
        }

        ColumnLayout {
            anchors.centerIn: parent
            width: Math.min(parent.width - 40, 420)
            visible: Player.state !== "ready"
            spacing: 12
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                visible: Player.state === "starting" || Player.state === "loading" || Player.state === "off"
                running: visible && !K.still
                implicitWidth: 34; implicitHeight: 34
            }
            Text {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: Player.state === "error" ? (I18n.s[Player.errorKey] || I18n.s.playerFailed)
                      : Player.state === "loading" ? I18n.s.playerLoading
                      : I18n.s.playerStarting
                color: Player.state === "error" ? Theme.c.danger : "#c8c8c8"
                font.family: Theme.fontFamily
                font.pixelSize: 13
            }
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                visible: Player.state === "error"
                spacing: 8
                KButton {
                    small: true; kind: "ghost"
                    visible: Player.errorKey !== "playerBlocked" && Player.errorKey !== "playerUnavailable"
                    text: I18n.s.playerRetry
                    onClicked: Player.retry()
                }
                KButton {
                    small: true; kind: "primary"
                    iconName: "link"
                    text: I18n.s.stageOpenSource
                    onClicked: Links.openExternal(Room.source)
                }
            }
        }
    }

    // Native controls.
    RowLayout {
        Layout.fillWidth: true
        spacing: 10
        enabled: Player.state === "ready"
        opacity: enabled ? 1 : 0.5
        IconButton {
            iconName: Player.playing ? "pause" : "play"
            active: Player.playing
            tip: Player.playing ? I18n.s.playerPause : I18n.s.playerPlay
            onClicked: Player.togglePlay()
        }
        Text {
            text: box.clock(seek.pressed ? seek.value : Player.position)
                  + (Player.duration > 0 ? "  /  " + box.clock(Player.duration) : "")
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            font.features: { "tnum": 1 }
        }
        Slider {
            id: seek
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, Player.duration)
            enabled: Room.canControl && Player.duration > 0
            value: pressed ? value : Player.position
            onPressedChanged: if (!pressed) Player.seek(value)
            ToolTip.visible: hovered && !Room.canControl
            ToolTip.delay: 450
            ToolTip.text: I18n.s.playerHostOnly
            Accessible.name: I18n.s.stageSource
            background: Rectangle {
                x: seek.leftPadding
                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                width: seek.availableWidth
                height: 4
                radius: 2
                color: K.stroke
                Rectangle {
                    width: seek.visualPosition * parent.width
                    height: parent.height
                    radius: 2
                    color: Theme.c.primary
                }
            }
            handle: Rectangle {
                x: seek.leftPadding + seek.visualPosition * (seek.availableWidth - width)
                y: seek.topPadding + seek.availableHeight / 2 - height / 2
                width: 14; height: 14; radius: 7
                visible: seek.enabled
                color: Theme.c.primary
                scale: seek.pressed ? 1.2 : 1
                Behavior on scale { NumberAnimation { duration: K.fast } }
            }
        }
        Icon { name: "headphones"; size: 16; color: K.muted }
        Slider {
            id: volume
            implicitWidth: 90
            from: 0; to: 100
            value: Player.volume
            onMoved: Player.volume = Math.round(value)
            Accessible.name: I18n.s.playerVolume
            background: Rectangle {
                x: volume.leftPadding
                y: volume.topPadding + volume.availableHeight / 2 - height / 2
                width: volume.availableWidth
                height: 4
                radius: 2
                color: K.stroke
                Rectangle { width: volume.visualPosition * parent.width; height: parent.height; radius: 2; color: K.muted }
            }
            handle: Rectangle {
                x: volume.leftPadding + volume.visualPosition * (volume.availableWidth - width)
                y: volume.topPadding + volume.availableHeight / 2 - height / 2
                width: 12; height: 12; radius: 6
                color: Theme.c.foreground
            }
        }
    }
    Text {
        Layout.fillWidth: true
        visible: Room.playback === "" && Player.state === "ready"
        text: I18n.s.playerIndependent
        color: K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
}
