import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Katsodi.App
import Katsodi.Ui

// Link demo (screen sharing) in the room's stage, by the website's rules:
// ask to show a link; hosts accept, pause or end; the presenter picks a
// screen or window; everyone in the room watches it here.
ColumnLayout {
    id: panel
    spacing: 10
    readonly property bool running: Room.demoStatus === "live" || Room.demoStatus === "paused"
    readonly property bool pending: Room.demoStatus === "pending"

    // Watching costs decoding: only while this panel is on screen.
    onVisibleChanged: Demo.setWanted(visible)
    Component.onCompleted: { Demo.setWanted(visible); Demo.setVideoSink(video.videoSink) }
    Component.onDestruction: { Demo.setWanted(false); Demo.setVideoSink(null) }

    // Nothing running: ask to show a link.
    RowLayout {
        Layout.fillWidth: true
        visible: Room.demoStatus === "" && Room.joined && !Room.closed && Demo.available
        spacing: 8
        KTextField {
            id: demoField
            Layout.fillWidth: true
            iconName: "globe"
            placeholderText: I18n.s.demoPlaceholder
            maximumLength: 2048
            onAccepted: if (Demo.request(text)) demoField.clear()
        }
        KButton {
            kind: "ghost"
            iconName: "sliders"
            text: Demo.busy ? I18n.s.loading : I18n.s.demoAsk
            enabled: !Demo.busy && demoField.text.trim().length > 0
            onClicked: if (Demo.request(demoField.text)) demoField.clear()
        }
    }

    // A request waiting for a host.
    Glass {
        Layout.fillWidth: true
        visible: panel.pending
        implicitHeight: pendingRow.implicitHeight + 24
        radius: 14
        RowLayout {
            id: pendingRow
            anchors.fill: parent
            anchors.margins: 12
            spacing: 10
            Icon { name: "clock"; size: 18; color: Theme.c.accent }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: Room.demoMine ? I18n.s.demoPendingMine : I18n.s.demoPendingHost.replace("{name}", Room.demoUserName)
                    color: K.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    wrapMode: Text.WordWrap
                }
                Text {
                    Layout.fillWidth: true
                    text: Room.demoUrl
                    color: K.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 11
                    elide: Text.ElideMiddle
                }
            }
            KButton {
                visible: Room.canControl && !Room.demoMine
                small: true; kind: "primary"
                text: I18n.s.demoAccept
                enabled: !Demo.busy
                onClicked: Demo.decide(true)
            }
            KButton {
                visible: Room.canControl && !Room.demoMine
                small: true; kind: "ghost"
                text: I18n.s.demoDecline
                enabled: !Demo.busy
                onClicked: Demo.decide(false)
            }
            KButton {
                visible: Room.demoMine
                small: true; kind: "ghost"
                text: I18n.s.demoWithdraw
                enabled: !Demo.busy
                onClicked: Demo.stop()
            }
        }
    }

    // A running demo: header with controls, then the screen.
    RowLayout {
        Layout.fillWidth: true
        visible: panel.running
        spacing: 8
        Rectangle {
            width: 8; height: 8; radius: 4
            color: Room.demoStatus === "live" ? Theme.c.danger : K.muted
        }
        Text {
            Layout.fillWidth: true
            text: Demo.sharing ? I18n.s.demoYouShare
                  : Room.demoStatus === "paused" ? I18n.s.demoPausedBy
                  : I18n.s.demoLive.replace("{name}", Room.demoUserName)
            color: K.text
            font.family: Theme.fontFamily
            font.pixelSize: 13
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        IconButton {
            iconName: "link"
            diameter: 32
            tip: I18n.s.demoOpenLink
            visible: Room.demoUrl !== ""
            onClicked: Links.openExternal(Room.demoUrl)
        }
        KButton {
            visible: Room.canControl
            small: true; kind: "ghost"
            text: Room.demoStatus === "paused" ? I18n.s.demoResume : I18n.s.demoPause
            enabled: !Demo.busy
            onClicked: Demo.togglePause()
        }
        KButton {
            visible: Demo.presenter && !Demo.sharing
            small: true; kind: "primary"
            text: I18n.s.demoShare
            onClicked: { Demo.refreshSources(); picker.open() }
        }
        KButton {
            visible: Demo.sharing
            small: true; kind: "primary"
            text: I18n.s.demoStopSharing
            onClicked: Demo.stopSharing()
        }
        KButton {
            visible: (Room.canControl || Room.demoMine) && !Demo.sharing
            small: true; kind: "ghost"
            text: I18n.s.demoEnd
            enabled: !Demo.busy
            onClicked: Demo.stop()
        }
    }

    Rectangle {
        Layout.fillWidth: true
        visible: panel.running
        Layout.preferredHeight: Math.min(Math.round(width / Math.max(0.5, Demo.remoteAspect)), 620)
        radius: 14
        color: "#000000"
        border.width: 1
        border.color: K.stroke
        clip: true

        VideoOutput {
            id: video
            anchors.fill: parent
            anchors.margins: 1
            fillMode: VideoOutput.PreserveAspectFit
            visible: Demo.remote
        }
        Text {
            anchors.centerIn: parent
            width: parent.width - 40
            visible: !Demo.remote
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Demo.state === "error" ? (I18n.s[Demo.errorKey] || I18n.s.screenUnavailable)
                  : Demo.sharing ? I18n.s.demoYouShare
                  : Demo.presenter ? I18n.s.demoChoose
                  : I18n.s.demoWaiting.replace("{name}", Room.demoUserName)
            color: Demo.state === "error" ? Theme.c.danger : "#c8c8c8"
            font.family: Theme.fontFamily
            font.pixelSize: 13
        }
    }
    Text {
        Layout.fillWidth: true
        visible: Demo.sharing || (Demo.errorKey !== "" && Demo.state !== "error")
        text: Demo.errorKey !== "" ? (I18n.s[Demo.errorKey] || I18n.s.errorServer) : I18n.s.demoNoAudio
        color: Demo.errorKey !== "" ? Theme.c.danger : K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    // The provider player is a native window: hide it under the picker.
    Binding { target: Player; property: "obscured"; value: true; when: picker.visible }

    // Presenter's choice of a screen or window.
    Popup {
        id: picker
        parent: Overlay.overlay
        modal: true
        x: (parent.width - width) / 2
        y: Math.min(100, parent.height * 0.1)
        width: Math.min(560, parent.width - 32)
        height: Math.min(520, parent.height - y - 24)
        padding: 0
        background: Glass { solid: true; radius: K.rPanel }
        Overlay.modal: Rectangle { color: Qt.alpha(Theme.c.background, 0.8) }
        contentItem: ColumnLayout {
            spacing: 8
            Text {
                Layout.fillWidth: true
                Layout.margins: 18
                Layout.bottomMargin: 0
                text: I18n.s.demoChoose
                color: K.text
                font.family: Theme.displayFamily
                font.pixelSize: 24
                font.weight: Font.Bold
            }
            ListView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.bottomMargin: 10
                clip: true
                model: Demo.sources
                spacing: 2
                ScrollBar.vertical: ScrollBar {}
                delegate: AbstractButton {
                    id: choice
                    required property var modelData
                    width: ListView.view.width
                    height: 52
                    hoverEnabled: true
                    onClicked: { Demo.share(modelData.index); picker.close() }
                    Accessible.name: modelData.name
                    background: Rectangle {
                        radius: 12
                        x: 8; width: parent.width - 16
                        color: choice.hovered ? K.glassHover : "transparent"
                    }
                    contentItem: RowLayout {
                        spacing: 12
                        Item { implicitWidth: 10 }
                        Icon { name: choice.modelData.kind === "screen" ? "video" : "rooms"; size: 18; color: Theme.c.accent }
                        Text {
                            Layout.fillWidth: true
                            text: choice.modelData.name
                            color: K.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                            elide: Text.ElideRight
                        }
                        Text {
                            text: choice.modelData.kind === "screen" ? I18n.s.demoScreen : I18n.s.demoWindow
                            color: K.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                        }
                        Item { implicitWidth: 10 }
                    }
                }
            }
        }
    }
}
