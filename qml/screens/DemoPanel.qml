import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import Katsodi.App
import Katsodi.Ui

// The room's Demo pane (screen sharing), by the website's rules: ask to
// show a link; hosts accept, pause or end; the presenter picks a window or
// a screen (Discord-style picker below) and sees their own picture; everyone
// else in the room watches it here.
ColumnLayout {
    id: panel
    spacing: 12
    readonly property bool running: Room.demoStatus === "live" || Room.demoStatus === "paused"
    readonly property bool pending: Room.demoStatus === "pending"

    // Watching costs decoding: only while this pane is on screen.
    onVisibleChanged: Demo.setWanted(visible)
    Component.onCompleted: {
        Demo.setWanted(visible)
        Demo.setVideoSink(remoteVideo.videoSink)
        Demo.setPreviewSink(selfVideo.videoSink)
    }
    Component.onDestruction: {
        Demo.setWanted(false)
        Demo.setVideoSink(null)
        Demo.setPreviewSink(null)
    }

    // --- Nothing running: what a demo is, and the link to ask for ---------
    ColumnLayout {
        Layout.fillWidth: true
        Layout.fillHeight: true
        visible: Room.demoStatus === ""
        spacing: 14
        Item { Layout.fillHeight: true }
        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            implicitWidth: 72; implicitHeight: 72; radius: 22
            color: Theme.c.selected
            Icon { anchors.centerIn: parent; name: "monitor"; size: 32; color: Theme.c.accent }
        }
        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 520
            Layout.alignment: Qt.AlignHCenter
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: I18n.s.demoIdle
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.alignment: Qt.AlignHCenter
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
                kind: "primary"
                iconName: "monitor"
                text: Demo.busy ? I18n.s.loading : I18n.s.demoAsk
                enabled: !Demo.busy && demoField.text.trim().length > 0
                onClicked: if (Demo.request(demoField.text)) demoField.clear()
            }
        }
        Text {
            Layout.alignment: Qt.AlignHCenter
            visible: Demo.errorKey !== ""
            text: I18n.s[Demo.errorKey] || I18n.s.errorServer
            color: Theme.c.danger
            font.family: Theme.fontFamily
            font.pixelSize: 12
        }
        Item { Layout.fillHeight: true }
    }

    // --- A request waiting for a host -------------------------------------
    Glass {
        Layout.fillWidth: true
        visible: panel.pending
        implicitHeight: pendingRow.implicitHeight + 28
        radius: 16
        RowLayout {
            id: pendingRow
            anchors.fill: parent
            anchors.margins: 14
            spacing: 12
            Icon { name: "clock"; size: 20; color: Theme.c.accent }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: Room.demoMine ? I18n.s.demoPendingMine : I18n.s.demoPendingHost.replace("{name}", Room.demoUserName)
                    color: K.text
                    font.family: Theme.fontFamily
                    font.pixelSize: 14
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
    Item { Layout.fillHeight: true; visible: panel.pending }

    // --- A running demo ---------------------------------------------------
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
            font.pixelSize: 14
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        IconButton {
            iconName: "link"
            diameter: 34
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
            iconName: "monitor"
            text: I18n.s.demoShare
            onClicked: picker.open()
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
        Layout.fillHeight: true
        Layout.minimumHeight: 220
        visible: panel.running
        radius: 16
        color: "#000000"
        border.width: 1
        border.color: K.stroke
        clip: true

        // What everyone else receives.
        VideoOutput {
            id: remoteVideo
            anchors.fill: parent
            anchors.margins: 1
            fillMode: VideoOutput.PreserveAspectFit
            visible: Demo.remote && !Demo.sharing
        }
        // The presenter's own picture, straight from the capture.
        VideoOutput {
            id: selfVideo
            anchors.fill: parent
            anchors.margins: 1
            fillMode: VideoOutput.PreserveAspectFit
            visible: Demo.sharing
        }
        Rectangle {
            visible: Demo.sharing
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 12
            radius: 12
            height: 26
            width: selfLabel.implicitWidth + 22
            color: Qt.alpha("#000000", 0.6)
            Text {
                id: selfLabel
                anchors.centerIn: parent
                text: I18n.s.demoSelfPreview
                color: "#ffffff"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }
        Text {
            anchors.centerIn: parent
            width: parent.width - 40
            visible: !Demo.sharing && !Demo.remote
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: Demo.state === "error" ? (I18n.s[Demo.errorKey] || I18n.s.screenUnavailable)
                  : Demo.presenter ? I18n.s.demoChoose
                  : I18n.s.demoWaiting.replace("{name}", Room.demoUserName)
            color: Demo.state === "error" ? Theme.c.danger : "#c8c8c8"
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
    }
    Text {
        Layout.fillWidth: true
        visible: panel.running && (Demo.sharing || (Demo.errorKey !== "" && Demo.state !== "error"))
        text: Demo.errorKey !== "" ? (I18n.s[Demo.errorKey] || I18n.s.errorServer)
              : I18n.s.demoNoAudio + "  ·  " + I18n.s.demoQualityNote.replace("{size}", Demo.quality + "p").replace("{fps}", Demo.framesPerSecond)
        color: Demo.errorKey !== "" ? Theme.c.danger : K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    // The provider player is a native window: hide it under the picker.
    Binding { target: Player; property: "obscured"; value: true; when: picker.visible }

    SharePicker { id: picker }
}
