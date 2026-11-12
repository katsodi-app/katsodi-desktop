import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// The room's voice lounge (website voice-lounge.tsx): explicit join with
// the microphone off, mute, deafen, devices, speaking state and leave.
Rectangle {
    id: panel
    readonly property bool active: Voice.state !== "idle"
    implicitHeight: content.implicitHeight + 28
    radius: 14
    color: Theme.c.surfaceElevated

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 14
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            Rectangle {
                implicitWidth: 34; implicitHeight: 34; radius: 11
                color: Theme.c.selected
                // Sound-wave mark, as on the website's voice symbol.
                Row {
                    anchors.centerIn: parent
                    spacing: 2
                    Repeater {
                        model: [8, 14, 18, 12, 8]
                        delegate: Rectangle {
                            required property int modelData
                            width: 2; radius: 1
                            height: modelData
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.c.primary
                        }
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 1
                Text {
                    Layout.fillWidth: true
                    elide: Text.ElideRight
                    text: I18n.s.voiceTitle
                    color: Theme.c.foreground
                    font.pixelSize: 15
                    font.weight: Font.DemiBold
                }
                RowLayout {
                    spacing: 6
                    Rectangle {
                        width: 6; height: 6; radius: 3
                        color: Voice.state === "connected" ? Theme.c.success
                               : Voice.state === "idle" ? Theme.c.mutedForeground : Theme.c.warning
                    }
                    Text {
                        text: Voice.state === "connected" ? I18n.s.voiceConnected
                              : Voice.state === "joining" ? I18n.s.voiceJoining
                              : Voice.state === "reconnecting" ? I18n.s.voiceReconnecting
                              : I18n.s.voiceReady
                        color: Theme.c.mutedForeground
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Errors and the listen-first notice.
        Text {
            Layout.fillWidth: true
            visible: text.length > 0
            text: Voice.errorKey ? (I18n.s[Voice.errorKey] || I18n.s.voiceNetwork)
                  : !Voice.available ? I18n.s.voiceUnsupported
                  : !panel.active && !Room.joined ? I18n.s.voiceJoinRoom
                  : Voice.state === "connected" && !Voice.microphone && !Voice.deafened ? I18n.s.voiceListenFirst
                  : ""
            color: Voice.errorKey ? Theme.c.danger : Theme.c.mutedForeground
            font.pixelSize: 12
            wrapMode: Text.WordWrap
            Accessible.role: Voice.errorKey ? Accessible.AlertMessage : Accessible.StaticText
        }

        KButton {
            visible: !panel.active
            small: true
            text: I18n.s.voiceJoin
            enabled: Voice.available && Room.joined && !Room.closed
            onClicked: Voice.join()
        }

        // Participants: speaking ring, microphone state.
        Flow {
            Layout.fillWidth: true
            visible: Voice.peers.length > 0
            spacing: 10
            Repeater {
                model: Voice.peers
                delegate: ColumnLayout {
                    required property var modelData
                    spacing: 4
                    width: 58
                    Item {
                        Layout.alignment: Qt.AlignHCenter
                        implicitWidth: 44; implicitHeight: 44
                        Rectangle {
                            anchors.fill: parent
                            radius: 22
                            color: Theme.c.selected
                            border.width: modelData.speaking ? 3 : 0
                            border.color: Theme.c.accent
                            Text {
                                anchors.centerIn: parent
                                text: modelData.initial
                                color: Theme.c.accent
                                font.weight: Font.Bold
                                font.pixelSize: 15
                            }
                        }
                        Rectangle {
                            visible: !modelData.microphone
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            width: 16; height: 16; radius: 8
                            color: Theme.c.surface
                            border.width: 1
                            border.color: Theme.c.border
                            Text { anchors.centerIn: parent; text: "×"; color: Theme.c.danger; font.pixelSize: 11; font.weight: Font.Bold }
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData.self ? modelData.name + " (" + I18n.s.voiceYou + ")" : modelData.name
                        color: Theme.c.foreground
                        font.pixelSize: 11
                        elide: Text.ElideRight
                    }
                    Accessible.role: Accessible.StaticText
                    Accessible.name: modelData.name + ", " + (modelData.speaking ? I18n.s.voiceSpeaking
                                                              : modelData.microphone ? I18n.s.voiceListening : I18n.s.voiceMuted)
                }
            }
        }

        // Call controls.
        Flow {
            Layout.fillWidth: true
            visible: Voice.state === "connected" || Voice.state === "reconnecting"
            spacing: 8
            KButton {
                small: true
                kind: Voice.microphone ? "secondary" : "primary"
                text: Voice.microphone ? I18n.s.voiceMute : I18n.s.voiceUnmute
                enabled: Voice.state === "connected" && !Voice.deafened
                onClicked: Voice.toggleMicrophone()
            }
            KButton {
                small: true
                kind: "secondary"
                text: Voice.deafened ? I18n.s.voiceUndeafen : I18n.s.voiceDeafen
                enabled: Voice.state === "connected"
                onClicked: Voice.toggleDeafen()
            }
            KButton {
                small: true
                kind: "ghost"
                text: I18n.s.voiceDevices
                onClicked: { Voice.refreshDevices(); devices.open() }
            }
            KButton {
                small: true
                kind: "ghost"
                text: I18n.s.voiceLeave
                onClicked: Voice.leave()
            }
        }
        Text {
            visible: panel.active
            text: I18n.s.voiceNoRecording
            color: Theme.c.mutedForeground
            font.pixelSize: 10
        }
    }

    // Device choice; the system default is used until one is picked.
    Popup {
        id: devices
        y: panel.height + 6
        width: panel.width
        padding: 16
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        background: Rectangle {
            radius: 12
            color: Theme.c.surface
            border.width: 1
            border.color: Theme.c.borderStrong
        }
        contentItem: ColumnLayout {
            spacing: 8
            Text { text: I18n.s.voiceInput; color: Theme.c.foreground; font.pixelSize: 12; font.weight: Font.DemiBold }
            ComboBox {
                Layout.fillWidth: true
                model: Voice.inputs
                textRole: "name"
                valueRole: "id"
                onActivated: Voice.setInput(currentValue)
                Accessible.name: I18n.s.voiceInput
            }
            Text { text: I18n.s.voiceOutput; color: Theme.c.foreground; font.pixelSize: 12; font.weight: Font.DemiBold }
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
                color: Theme.c.mutedForeground
                font.pixelSize: 11
                wrapMode: Text.WordWrap
            }
        }
    }
}
