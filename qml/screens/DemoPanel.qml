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
        KTextField {
            id: demoField
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.alignment: Qt.AlignHCenter
            iconName: "globe"
            placeholderText: I18n.s.demoPlaceholder
            maximumLength: 2048
            onAccepted: if (Demo.request(text)) demoField.clear()
        }
        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: 8
            KButton {
                kind: "primary"
                iconName: "monitor"
                text: Demo.busy ? I18n.s.loading : I18n.s.demoAsk
                enabled: !Demo.busy && demoField.text.trim().length > 0
                onClicked: if (Demo.request(demoField.text)) demoField.clear()
            }
            KButton {
                visible: Room.canControl && Browser.available
                kind: "ghost"
                iconName: "globe"
                text: I18n.s.browserStart
                enabled: !Demo.busy && demoField.text.trim().length > 0
                onClicked: { disclaimer.url = demoField.text.trim(); disclaimer.open() }
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 620
            Layout.alignment: Qt.AlignHCenter
            visible: Room.canControl && Browser.available
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            text: I18n.s.browserStartHint
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
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
            text: Demo.browserSession ? (Browser.title || I18n.s.browserStart)
                  : Demo.sharing ? I18n.s.demoYouShare
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
            visible: Demo.browserSession
            small: true; kind: "ghost"
            iconName: "rooms"
            text: I18n.s.browserAccess + (Demo.controlAll ? " · ∞" : Demo.controllers.length ? " · " + Demo.controllers.length : "")
            onClicked: access.open()
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

    RowLayout {
        Layout.fillWidth: true
        visible: panel.running && Demo.browserSession
        spacing: 6
        IconButton { iconName: "back"; diameter: 34; tip: I18n.s.browserBack; enabled: Browser.canBack; onClicked: Browser.back() }
        IconButton { iconName: "chevron"; diameter: 34; tip: I18n.s.browserForward; enabled: Browser.canForward; onClicked: Browser.forward() }
        IconButton { iconName: "clock"; diameter: 34; tip: I18n.s.browserReload; onClicked: Browser.reload() }
        KTextField {
            id: address
            Layout.fillWidth: true
            iconName: "globe"
            placeholderText: I18n.s.browserUrlPlaceholder
            maximumLength: 2048
            text: Browser.url
            onAccepted: Browser.navigate(text)
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

        // The presenter's shared browser is drawn here by the helper process.
        Item {
            id: browserSurface
            anchors.fill: parent
            anchors.margins: 1
            visible: Demo.browserSession
            function report() {
                const window = browserSurface.Window.window
                if (!window)
                    return
                const p = browserSurface.mapToItem(null, 0, 0)
                const x = Math.max(0, p.x), y = Math.max(0, p.y)
                const w = Math.min(p.x + width, window.width) - x
                const h = Math.min(p.y + height, window.height) - y
                Browser.setSurface(window, x, y, Math.max(0, w), Math.max(0, h), browserSurface.visible && panel.visible && w > 40 && h > 40)
            }
            Connections {
                target: browserSurface.Window.window
                function onAfterAnimating() { if (Demo.browserSession) browserSurface.report() }
            }
            onVisibleChanged: report()
            onWidthChanged: report()
            onHeightChanged: report()
            Component.onDestruction: {
                const window = browserSurface.Window.window
                if (window)
                    Browser.setSurface(window, 0, 0, 0, 0, false)
            }
        }

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
            visible: Demo.sharing && !Demo.browserSession
        }
        // A viewer with control: clicks, wheel and keys go to the presenter's browser.
        MouseArea {
            id: remoteInput
            anchors.fill: remoteVideo
            visible: Demo.canControl && Demo.remote && !Demo.sharing
            enabled: visible
            hoverEnabled: true
            acceptedButtons: Qt.LeftButton | Qt.RightButton | Qt.MiddleButton
            cursorShape: Qt.PointingHandCursor
            property real lastMove: 0
            function point(mouse) {
                const r = remoteVideo.contentRect
                return { x: Math.max(0, Math.min(1, (mouse.x - r.x) / Math.max(1, r.width))),
                         y: Math.max(0, Math.min(1, (mouse.y - r.y) / Math.max(1, r.height))) }
            }
            function button(b) { return b === Qt.RightButton ? "right" : b === Qt.MiddleButton ? "middle" : "left" }
            onPositionChanged: (mouse) => {
                const now = Date.now()
                if (now - lastMove < 33)
                    return
                lastMove = now
                const p = point(mouse)
                Demo.sendInput({ type: "mouse", action: "move", x: p.x, y: p.y, button: "none" })
            }
            onPressed: (mouse) => {
                keys.forceActiveFocus()
                const p = point(mouse)
                Demo.sendInput({ type: "mouse", action: "down", x: p.x, y: p.y, button: button(mouse.button), clicks: 1 })
            }
            onReleased: (mouse) => {
                const p = point(mouse)
                Demo.sendInput({ type: "mouse", action: "up", x: p.x, y: p.y, button: button(mouse.button), clicks: 1 })
            }
            onWheel: (wheel) => {
                const p = point(wheel)
                Demo.sendInput({ type: "mouse", action: "wheel", x: p.x, y: p.y, dx: -wheel.angleDelta.x, dy: -wheel.angleDelta.y })
            }
        }
        Item {
            id: keys
            anchors.fill: remoteInput
            visible: remoteInput.visible
            focus: false
            // Text goes as text (any language); special keys as key events.
            readonly property var special: ({
                [Qt.Key_Return]: ["Enter", "Enter", 13], [Qt.Key_Enter]: ["Enter", "NumpadEnter", 13],
                [Qt.Key_Backspace]: ["Backspace", "Backspace", 8], [Qt.Key_Tab]: ["Tab", "Tab", 9],
                [Qt.Key_Escape]: ["Escape", "Escape", 27], [Qt.Key_Delete]: ["Delete", "Delete", 46],
                [Qt.Key_Left]: ["ArrowLeft", "ArrowLeft", 37], [Qt.Key_Up]: ["ArrowUp", "ArrowUp", 38],
                [Qt.Key_Right]: ["ArrowRight", "ArrowRight", 39], [Qt.Key_Down]: ["ArrowDown", "ArrowDown", 40],
                [Qt.Key_Home]: ["Home", "Home", 36], [Qt.Key_End]: ["End", "End", 35],
                [Qt.Key_PageUp]: ["PageUp", "PageUp", 33], [Qt.Key_PageDown]: ["PageDown", "PageDown", 34]
            })
            function modifiers(event) {
                return (event.modifiers & Qt.ControlModifier ? 2 : 0) | (event.modifiers & Qt.ShiftModifier ? 8 : 0)
            }
            function send(event, action) {
                const known = special[event.key]
                if (known) {
                    Demo.sendInput({ type: "key", action: action, key: known[0], code: known[1], vk: known[2],
                                     modifiers: modifiers(event), text: known[0] === "Enter" && action === "down" ? "\r" : "" })
                    return true
                }
                // Ctrl + letter (select all, undo, redo); the presenter filters the rest.
                if (event.modifiers & Qt.ControlModifier && event.key >= Qt.Key_A && event.key <= Qt.Key_Z) {
                    const letter = String.fromCharCode(event.key + 32)
                    Demo.sendInput({ type: "key", action: action, key: letter, code: "Key" + letter.toUpperCase(),
                                     vk: event.key, modifiers: modifiers(event) })
                    return true
                }
                if (action === "down" && event.text.length > 0 && event.text.charCodeAt(0) >= 32) {
                    Demo.sendInput({ type: "text", text: event.text })
                    return true
                }
                return false
            }
            Keys.onPressed: (event) => { event.accepted = send(event, "down") }
            Keys.onReleased: (event) => { event.accepted = send(event, "up") }
        }
        Rectangle {
            visible: Demo.browserSession === false && !Demo.sharing && Demo.remote && Room.demoStatus === "live"
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 12
            radius: 12
            height: 26
            width: controlLabel.implicitWidth + 22
            color: Qt.alpha("#000000", 0.6)
            Text {
                id: controlLabel
                anchors.centerIn: parent
                text: Demo.canControl ? I18n.s.browserYouControl : ""
                color: "#ffffff"
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.DemiBold
            }
        }
        Rectangle {
            visible: Demo.sharing && !Demo.browserSession
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
        // The presenter's sound, for viewers: on or off.
        IconButton {
            visible: Demo.remoteAudio && !Demo.sharing
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 12
            z: 2
            diameter: 36
            iconName: Demo.remoteAudioMuted ? "headphonesOff" : "headphones"
            tip: Demo.remoteAudioMuted ? I18n.s.demoSoundOn : I18n.s.demoSoundOff
            onClicked: Demo.remoteAudioMuted = !Demo.remoteAudioMuted
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
              : (({ window: I18n.s.demoAudioWindow, system: I18n.s.demoAudioSystem, unavailable: I18n.s.demoAudioUnavailable })[Demo.audioState]
                    || I18n.s.demoAudioOff)
                + "  ·  " + I18n.s.demoQualityNote.replace("{size}", Demo.quality + "p").replace("{fps}", Demo.framesPerSecond)
        color: Demo.errorKey !== "" ? Theme.c.danger : K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }

    Text {
        Layout.fillWidth: true
        visible: panel.running && !Demo.sharing && Demo.remote
        text: Demo.canControl ? I18n.s.browserYouControl : I18n.s.browserViewOnly
        color: Demo.canControl ? Theme.c.success : K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        font.weight: Font.DemiBold
    }

    // The player and the shared browser are native windows: they step aside
    // under these popups, since nothing can be drawn over them.
    Binding { target: Player; property: "obscured"; value: true; when: picker.visible || disclaimer.visible || access.visible }
    Binding { target: Browser; property: "obscured"; value: true; when: picker.visible || disclaimer.visible || access.visible }

    // Development capture mode only (RoomScreen.captureDemo).
    function captureDialog(name) {
        picker.close(); disclaimer.close(); access.close()
        if (name === "disclaimer") { disclaimer.url = "https://example.com/"; disclaimer.open() }
        else if (name === "access") access.open()
        else if (name === "picker") picker.open()
    }

    SharePicker { id: picker }
    BrowserDisclaimer {
        id: disclaimer
        onConfirmed: (url) => { if (Demo.startBrowser(url)) demoField.clear() }
    }
    BrowserAccess { id: access }
}
