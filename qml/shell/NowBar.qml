import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Floating bar of the open room (Liner's player bar, the website's room
// dock): which room you are in, the voice call with quick controls, a way
// back and a way out. It slides up when a room is open on another screen.
Glass {
    id: bar
    property bool shown: false
    signal returnRequested()
    strong: true
    radius: 24
    implicitHeight: 68
    opacity: shown ? 1 : 0
    visible: opacity > 0
    transform: Translate { y: bar.shown ? 0 : 24; Behavior on y { NumberAnimation { duration: K.base; easing.type: K.easing } } }
    Behavior on opacity { NumberAnimation { duration: K.base } }

    readonly property var tones: ({ orbit: [Theme.c.accent, Theme.c.info], wave: [Theme.c.info, Theme.c.success],
                                   moon: [Theme.c.warning, Theme.c.accent], sun: [Theme.c.danger, Theme.c.warning] })

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // Room tile.
        Rectangle {
            id: roomTile
            implicitWidth: 44; implicitHeight: 44
            radius: 12
            readonly property var room: Rooms.room(Room.roomId)
            readonly property var tone: bar.tones[room.art] || bar.tones.orbit
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0; color: Qt.alpha(roomTile.tone[0], 0.85) }
                GradientStop { position: 1; color: Qt.alpha(roomTile.tone[1], 0.55) }
            }
            Icon {
                anchors.centerIn: parent
                name: Room.format === "music" ? "music" : "video"
                size: 20
                color: Theme.c.primaryForeground
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 1
            Text {
                Layout.fillWidth: true
                text: Room.name || I18n.s.loading
                color: K.text
                font.family: Theme.fontFamily
                font.pixelSize: 14
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            Text {
                Layout.fillWidth: true
                text: Voice.state === "connected" ? I18n.s.voiceConnected + (Voice.microphone ? "" : " · " + I18n.s.voiceMuted)
                      : Voice.state === "joining" ? I18n.s.voiceJoining
                      : Voice.state === "reconnecting" ? I18n.s.voiceReconnecting
                      : I18n.s.nowInRoom
                color: Voice.state === "connected" ? Theme.c.success : K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
            }
        }
        IconButton {
            visible: Voice.state === "connected"
            iconName: Voice.microphone ? "mic" : "micOff"
            active: Voice.microphone
            tip: Voice.microphone ? I18n.s.voiceMute : I18n.s.voiceUnmute
            enabled: !Voice.deafened
            onClicked: Voice.toggleMicrophone()
        }
        IconButton {
            visible: Voice.state === "connected"
            iconName: Voice.deafened ? "headphonesOff" : "headphones"
            tip: Voice.deafened ? I18n.s.voiceUndeafen : I18n.s.voiceDeafen
            onClicked: Voice.toggleDeafen()
        }
        KButton {
            small: true
            kind: "primary"
            text: I18n.s.nowReturn
            onClicked: bar.returnRequested()
        }
        IconButton {
            iconName: "close"
            tip: I18n.s.nowLeave
            onClicked: {
                Voice.leave()
                Room.close()
            }
        }
    }
}
