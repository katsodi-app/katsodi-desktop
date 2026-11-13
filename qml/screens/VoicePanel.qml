import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// The room's voice lounge (website voice-lounge.tsx): explicit join with
// the microphone off, mute, deafen, devices, speaking state and leave.
Glass {
    id: panel
    readonly property bool active: Voice.state !== "idle"
    implicitHeight: content.implicitHeight + 32
    radius: K.rPanel

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            Rectangle {
                implicitWidth: 40; implicitHeight: 40; radius: 20
                color: Voice.state === "connected" ? Theme.c.primary : Theme.c.selected
                Behavior on color { ColorAnimation { duration: K.base } }
                Icon {
                    anchors.centerIn: parent
                    name: "wave"
                    size: 20
                    color: Voice.state === "connected" ? Theme.c.primaryForeground : Theme.c.accent
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: I18n.s.voiceTitle
                    color: K.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    font.weight: Font.Bold
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 7; height: 7; radius: 3.5
                        color: Voice.state === "connected" ? Theme.c.success
                               : Voice.state === "idle" ? K.muted : Theme.c.warning
                    }
                    Text {
                        text: Voice.state === "connected" ? I18n.s.voiceConnected
                              : Voice.state === "joining" ? I18n.s.voiceJoining
                              : Voice.state === "reconnecting" ? I18n.s.voiceReconnecting
                              : I18n.s.voiceReady
                        color: K.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                    }
                }
            }
        }

        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: Voice.errorKey ? (I18n.s[Voice.errorKey] || I18n.s.voiceNetwork)
                  : !Voice.available ? I18n.s.voiceUnsupported
                  : !panel.active && !Room.joined ? I18n.s.voiceJoinRoom
                  : Voice.state === "connected" && !Voice.microphone && !Voice.deafened ? I18n.s.voiceListenFirst
                  : ""
            color: Voice.errorKey ? Theme.c.danger : K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Accessible.role: Voice.errorKey ? Accessible.AlertMessage : Accessible.StaticText
        }

        KButton {
            visible: !panel.active
            kind: "accent"
            small: true
            iconName: "headphones"
            text: I18n.s.voiceJoin
            enabled: Voice.available && Room.joined && !Room.closed
            onClicked: Voice.join()
        }

        // Participants with a ring that breathes while they speak.
        Flow {
            Layout.fillWidth: true
            visible: Voice.peers.length > 0
            spacing: 12
            Repeater {
                model: Voice.peers
                delegate: Column {
                    required property var modelData
                    spacing: 5
                    width: 60
                    Item {
                        anchors.horizontalCenter: parent.horizontalCenter
                        width: 48; height: 48
                        Rectangle {
                            anchors.centerIn: parent
                            width: 48; height: 48; radius: 24
                            color: "transparent"
                            border.width: modelData.speaking ? 3 : 0
                            border.color: Theme.c.accent
                            scale: modelData.speaking && !K.still ? 1.08 : 1
                            Behavior on scale { NumberAnimation { duration: K.fast } }
                        }
                        Rectangle {
                            anchors.centerIn: parent
                            width: 40; height: 40; radius: 20
                            color: Theme.c.selected
                            Text {
                                anchors.centerIn: parent
                                text: modelData.initial
                                color: Theme.c.accent
                                font.family: Theme.displayFamily
                                font.pixelSize: 17
                                font.weight: Font.Bold
                            }
                        }
                        Rectangle {
                            visible: !modelData.microphone
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 18; height: 18; radius: 9
                            color: K.glassStrong
                            border.width: 1
                            border.color: K.stroke
                            Icon { anchors.centerIn: parent; size: 11; name: "micOff"; color: Theme.c.danger }
                        }
                    }
                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.self ? I18n.s.voiceYou : modelData.name
                        color: K.text
                        font.family: Theme.fontFamily
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.name + ", " + (modelData.speaking ? I18n.s.voiceSpeaking
                                                              : modelData.microphone ? I18n.s.voiceListening : I18n.s.voiceMuted)
                }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            visible: Voice.state === "connected" || Voice.state === "reconnecting"
            spacing: 8
            IconButton {
                iconName: Voice.microphone ? "mic" : "micOff"
                active: Voice.microphone
                tip: Voice.microphone ? I18n.s.voiceMute : I18n.s.voiceUnmute
                enabled: Voice.state === "connected" && !Voice.deafened
                onClicked: Voice.toggleMicrophone()
            }
            IconButton {
                iconName: Voice.deafened ? "headphonesOff" : "headphones"
                tip: Voice.deafened ? I18n.s.voiceUndeafen : I18n.s.voiceDeafen
                enabled: Voice.state === "connected"
                onClicked: Voice.toggleDeafen()
            }
            IconButton {
                iconName: "sliders"
                tip: I18n.s.voiceDevices
                onClicked: { Voice.refreshDevices(); devices.open() }
            }
            Item { Layout.fillWidth: true }
            KButton {
                small: true
                kind: "ghost"
                iconName: "leave"
                text: I18n.s.voiceLeave
                onClicked: Voice.leave()
            }
        }
        Text {
            visible: panel.active
            text: I18n.s.voiceNoRecording
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 10
        }
    }

    Popup {
        id: devices
        y: panel.height + 8
        width: panel.width
        padding: 18
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        enter: Transition { NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base } }
        background: Glass { solid: true; radius: K.rCard }
        contentItem: ColumnLayout {
            spacing: 8
            Text { text: I18n.s.voiceInput; color: K.text; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold }
            ComboBox {
                Layout.fillWidth: true
                model: Voice.inputs
                textRole: "name"
                valueRole: "id"
                onActivated: Voice.setInput(currentValue)
                Accessible.name: I18n.s.voiceInput
            }
            Text { text: I18n.s.voiceOutput; color: K.text; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold }
            ComboBox {
                Layout.fillWidth: true
                model: Voice.outputs
                textRole: "name"
                valueRole: "id"
                onActivated: Voice.setOutput(currentValue)
                Accessible.name: I18n.s.voiceOutput
            }
            Text {
                Layout.fillWidth: true
                text: I18n.s.voiceVolumeNote
                color: K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
