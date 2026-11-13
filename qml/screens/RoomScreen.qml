import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// A shared room. The room stays open when you go to another screen (the
// floating bar brings you back), like the website's room dock; leaving
// is explicit. Layout: stage with source, playback and queue; a side
// column with the voice lounge and the conversation.
Item {
    id: screen
    property string roomId
    signal back()
    signal navigate(string name, string id)
    readonly property bool narrow: width < 980
    readonly property int side: width < 760 ? 20 : 40

    Component.onCompleted: if (roomId && Room.roomId !== roomId.toLowerCase()) Room.open(roomId)
    onRoomIdChanged: if (roomId && Room.roomId !== roomId.toLowerCase()) Room.open(roomId)

    function clock(seconds) {
        const s = Math.max(0, Math.floor(seconds))
        const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60), r = s % 60
        return (h ? h + ":" + String(m).padStart(2, "0") : m) + ":" + String(r).padStart(2, "0")
    }

    // Wide windows: everything fits and fills the height. Narrow windows:
    // panels get fixed heights and the page scrolls.
    Scroller {
        id: page
        anchors.fill: parent
        contentHeight: screen.narrow ? column.implicitHeight + 46 : height
        interactive: screen.narrow

    ColumnLayout {
        id: column
        x: screen.side
        y: 26
        width: page.width - 2 * screen.side
        height: screen.narrow ? implicitHeight : page.height - 46
        spacing: 18

        RowLayout {
            Layout.fillWidth: true
            spacing: 14
            IconButton { iconName: "back"; tip: I18n.s.back; onClicked: screen.back() }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: Room.name || (Room.loading ? I18n.s.loading : "")
                    color: K.text
                    font.family: Theme.displayFamily
                    font.pixelSize: 40
                    font.weight: Font.Bold
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Text {
                    Layout.fillWidth: true
                    visible: Room.name !== ""
                    text: [Room.format === "music" ? I18n.s.roomMusic : I18n.s.roomVideo,
                           Room.visibility === "private" ? I18n.s.roomPrivate : I18n.s.roomPublic,
                           Room.fandomId ? Fandoms.nameOf(Room.fandomId) : I18n.s.roomOther].join("  ·  ")
                    color: K.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    elide: Text.ElideRight
                }
            }
            IconButton {
                visible: Room.name !== ""
                iconName: "bookmark"
                active: Library.version >= 0 && Library.isSaved(Room.roomId)
                tip: active ? I18n.s.roomUnsave : I18n.s.roomSave
                onClicked: Library.toggleSaved(Room.roomId)
            }
            KButton {
                visible: Room.name !== "" && !Room.joined && Account.signedIn && !Room.closed
                kind: "accent"
                text: Room.joining ? I18n.s.loading : I18n.s.roomJoin
                enabled: !Room.joining
                onClicked: Room.join()
            }
            KButton {
                visible: Room.name !== "" && !Account.signedIn
                kind: "ghost"
                text: I18n.s.signIn
                onClicked: screen.navigate("signin", "")
            }
        }

        Glass {
            Layout.fillWidth: true
            visible: Room.errorKey !== "" || Room.actionErrorKey !== ""
            implicitHeight: notice.implicitHeight + 24
            RowLayout {
                id: notice
                anchors.fill: parent
                anchors.margins: 12
                anchors.leftMargin: 18
                Text {
                    Layout.fillWidth: true
                    text: I18n.s[Room.errorKey || Room.actionErrorKey] || I18n.s.errorServer
                    color: Room.errorKey === "roomClosed" ? K.muted : Theme.c.danger
                    font.family: Theme.fontFamily
                    wrapMode: Text.WordWrap
                }
                KButton {
                    visible: Room.errorKey !== "" && Room.errorKey !== "roomClosed"
                    kind: "ghost"; small: true
                    text: I18n.s.retry
                    onClicked: Room.retry()
                }
            }
        }

        Item { Layout.fillHeight: true; visible: Room.name === "" && !screen.narrow }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: !screen.narrow
            visible: Room.name !== ""
            columns: screen.narrow ? 1 : 2
            columnSpacing: 18
            rowSpacing: 18

            // Stage: source, timeline and queue.
            Glass {
                id: stage
                Layout.fillWidth: true
                Layout.fillHeight: !screen.narrow
                Layout.preferredHeight: screen.narrow ? 420 : -1
                Layout.minimumHeight: 260
                radius: K.rPanel
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 22
                    spacing: 16

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 14
                        Rectangle {
                            implicitWidth: 64; implicitHeight: 64; radius: 16
                            color: Theme.c.selected
                            Icon { anchors.centerIn: parent; size: 28; name: Room.format === "music" ? "music" : "video"; color: Theme.c.accent }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 3
                            Text {
                                text: I18n.s.stageSource.toUpperCase()
                                color: K.muted
                                font.family: Theme.fontFamily
                                font.pixelSize: 10
                                font.weight: Font.Bold
                                font.letterSpacing: 1.4
                            }
                            Text {
                                Layout.fillWidth: true
                                text: Room.source ? Links.host(Room.source) : I18n.s.stageNoSource
                                color: K.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 20
                                font.weight: Font.Bold
                                elide: Text.ElideMiddle
                            }
                            // Shared timeline from the server (synchronized sources).
                            RowLayout {
                                spacing: 8
                                visible: Room.source !== ""
                                Rectangle {
                                    width: 8; height: 8; radius: 4
                                    color: Room.playback === "playing" ? Theme.c.success : K.muted
                                    SequentialAnimation on opacity {
                                        running: Room.playback === "playing" && !K.still
                                        loops: Animation.Infinite
                                        NumberAnimation { to: 0.3; duration: 900 }
                                        NumberAnimation { to: 1; duration: 900 }
                                    }
                                }
                                Text {
                                    id: timeline
                                    property real base: Room.playbackPosition
                                    property real ticks: 0
                                    text: Room.playback === "playing" ? I18n.s.playbackPlaying + "  ·  " + screen.clock(base + ticks)
                                          : Room.playback === "paused" ? I18n.s.playbackPaused + "  ·  " + screen.clock(base)
                                          : I18n.s.playbackNone
                                    color: K.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 12
                                    onBaseChanged: ticks = 0
                                    // One tick per second, only while playing and visible.
                                    Timer {
                                        interval: 1000
                                        repeat: true
                                        running: Room.playback === "playing" && screen.visible
                                        onTriggered: timeline.ticks += 1
                                    }
                                }
                            }
                        }
                        KButton {
                            visible: Room.source !== "" && stage.width >= 620
                            kind: "ghost"; small: true
                            iconName: "link"
                            text: I18n.s.stageOpenSource
                            onClicked: Links.openExternal(Room.source)
                        }
                        IconButton {
                            visible: Room.source !== "" && stage.width < 620
                            iconName: "link"
                            tip: I18n.s.stageOpenSource
                            onClicked: Links.openExternal(Room.source)
                        }
                    }
                    PlayerStage {
                        Layout.fillWidth: true
                        visible: Room.syncKind !== "" && Player.available
                        maxVideoHeight: Math.max(180, stage.height * 0.55)
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: Room.source !== "" && !(Room.syncKind !== "" && Player.available)
                        text: Room.syncKind === "" ? I18n.s.playerOpenOutside : I18n.s.playerUnavailable
                        color: K.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: K.stroke }

                    SectionTitle {
                        Layout.fillWidth: true
                        iconName: "queue"
                        title: I18n.s.queueTitle + (Room.queue.length ? " · " + Room.queue.length : "")
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        visible: Room.joined && !Room.closed
                        spacing: 8
                        KTextField {
                            id: queueField
                            Layout.fillWidth: true
                            iconName: "link"
                            placeholderText: I18n.s.queuePlaceholder
                            maximumLength: 2048
                            onAccepted: if (Room.addToQueue(text)) queueField.clear()
                        }
                        KButton {
                            text: Room.queueing ? I18n.s.loading : I18n.s.queueAdd
                            enabled: !Room.queueing && queueField.text.trim().length > 0
                            onClicked: if (Room.addToQueue(queueField.text)) queueField.clear()
                        }
                    }
                    ListView {
                        id: queueList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: 60
                        clip: true
                        spacing: 4
                        model: Room.queue
                        boundsBehavior: Flickable.StopAtBounds
                        ScrollBar.vertical: ScrollBar {}
                        delegate: Rectangle {
                            required property var modelData
                            required property int index
                            width: queueList.width - 6
                            height: 48
                            radius: 12
                            color: rowHover.hovered ? K.glassHover : "transparent"
                            HoverHandler { id: rowHover }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 12
                                Text {
                                    text: index + 1
                                    color: K.muted
                                    font.family: Theme.displayFamily
                                    font.pixelSize: 16
                                    Layout.preferredWidth: 22
                                }
                                ColumnLayout {
                                    Layout.fillWidth: true
                                    spacing: 0
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.host || modelData.url
                                        color: K.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.url
                                        color: K.muted
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 11
                                        elide: Text.ElideMiddle
                                    }
                                }
                                Text {
                                    visible: (modelData.addedBy || "") !== ""
                                    text: modelData.addedBy || ""
                                    color: K.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 11
                                }
                                IconButton {
                                    visible: Room.canControl
                                    diameter: 32
                                    iconName: "play"
                                    tip: I18n.s.queuePlayNow
                                    enabled: !Room.queueing
                                    onClicked: Room.playFromQueue(modelData.id)
                                }
                                IconButton {
                                    visible: modelData.mine || Room.canControl
                                    diameter: 32
                                    iconName: "trash"
                                    tip: I18n.s.queueRemove
                                    enabled: !Room.queueing
                                    onClicked: Room.removeFromQueue(modelData.id)
                                }
                            }
                        }
                        Text {
                            anchors.centerIn: parent
                            visible: queueList.count === 0
                            text: I18n.s.queueEmpty
                            color: K.muted
                            font.family: Theme.fontFamily
                        }
                    }
                }
            }

            // Side column: voice lounge and conversation.
            ColumnLayout {
                Layout.fillHeight: !screen.narrow
                Layout.fillWidth: screen.narrow
                Layout.preferredWidth: screen.narrow ? -1 : 380
                spacing: 18

                VoicePanel { Layout.fillWidth: true }

                Glass {
                    Layout.fillWidth: true
                    Layout.fillHeight: !screen.narrow
                    Layout.preferredHeight: screen.narrow ? 520 : -1
                    Layout.minimumHeight: 300
                    radius: K.rPanel
                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 14
                        spacing: 10

                        Segmented {
                            id: tabs
                            Layout.alignment: Qt.AlignHCenter
                            model: [{ value: "chat", label: I18n.s.roomChat },
                                    { value: "members", label: I18n.s.roomMembers + " · " + Room.members.length }]
                            current: "chat"
                            onSelected: (value) => current = value
                        }

                        StackLayout {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            currentIndex: tabs.current === "chat" ? 0 : 1

                            ColumnLayout {
                                spacing: 10
                                ListView {
                                    id: chat
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    clip: true
                                    model: Room.chat
                                    spacing: 2
                                    reuseItems: true
                                    boundsBehavior: Flickable.StopAtBounds
                                    property bool pinned: true
                                    onMovementEnded: pinned = atYEnd
                                    onCountChanged: if (pinned) Qt.callLater(positionViewAtEnd)
                                    ScrollBar.vertical: ScrollBar {}
                                    add: Transition {
                                        NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base }
                                    }
                                    delegate: Item {
                                        id: message
                                        required property string author
                                        required property string body
                                        required property string kind
                                        required property var time
                                        required property bool grouped
                                        required property bool own
                                        width: chat.width - 8
                                        height: bubble.implicitHeight + (grouped ? 2 : 8)
                                        Column {
                                            id: bubble
                                            width: parent.width
                                            anchors.bottom: parent.bottom
                                            spacing: 3
                                            Row {
                                                visible: !message.grouped
                                                spacing: 8
                                                topPadding: 6
                                                Text {
                                                    text: message.author
                                                    color: message.own ? Theme.c.accent : K.text
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: 12
                                                    font.weight: Font.Bold
                                                }
                                                Text {
                                                    text: Qt.formatTime(message.time, "HH:mm")
                                                    color: K.muted
                                                    font.family: Theme.fontFamily
                                                    font.pixelSize: 11
                                                }
                                            }
                                            Text {
                                                width: parent.width
                                                // Plain text: no markup from other users is interpreted.
                                                textFormat: Text.PlainText
                                                text: message.kind === "sticker" ? "✦ " + I18n.s.stickerSent
                                                      : message.kind === "image" ? "▣ " + I18n.s.imageMessage
                                                      : message.body
                                                color: message.kind === "text" ? K.text : K.muted
                                                font.family: Theme.fontFamily
                                                font.pixelSize: 14
                                                wrapMode: Text.Wrap
                                            }
                                        }
                                    }
                                    Text {
                                        anchors.centerIn: parent
                                        width: parent.width - 32
                                        visible: chat.count === 0 && !Room.loading && Room.joined
                                        text: I18n.s.roomNoMessages
                                        color: K.muted
                                        font.family: Theme.fontFamily
                                        horizontalAlignment: Text.AlignHCenter
                                        wrapMode: Text.WordWrap
                                    }
                                }

                                Text {
                                    Layout.fillWidth: true
                                    visible: !Room.joined
                                    text: Account.signedIn ? I18n.s.roomJoin + " — " + I18n.s.roomChat : I18n.s.roomSignInToJoin
                                    color: K.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    visible: Room.joined && !Room.closed
                                    spacing: 8
                                    ScrollView {
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Math.min(120, Math.max(46, composer.implicitHeight))
                                        TextArea {
                                            id: composer
                                            placeholderText: I18n.s.roomComposer
                                            placeholderTextColor: K.muted
                                            color: K.text
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 14
                                            wrapMode: TextArea.Wrap
                                            leftPadding: 16
                                            rightPadding: 16
                                            topPadding: 13
                                            bottomPadding: 13
                                            Accessible.name: I18n.s.roomComposer
                                            background: Rectangle {
                                                radius: 23
                                                color: composer.activeFocus ? K.glassHover : K.glass
                                                border.width: composer.activeFocus ? 2 : 1
                                                border.color: composer.activeFocus ? Theme.c.focusRing : K.stroke
                                            }
                                            // Enter sends; Shift+Enter adds a line; IME composition untouched.
                                            Keys.onReturnPressed: (event) => {
                                                if (event.modifiers & Qt.ShiftModifier || composer.inputMethodComposing) {
                                                    event.accepted = false
                                                    return
                                                }
                                                Room.send(composer.text)
                                            }
                                        }
                                    }
                                    IconButton {
                                        diameter: 46
                                        iconName: "chevron"
                                        active: composer.text.trim().length > 0
                                        tip: Room.sending ? I18n.s.roomSending : I18n.s.roomSend
                                        enabled: !Room.sending && composer.text.trim().length > 0 && composer.text.trim().length <= 2000
                                        onClicked: Room.send(composer.text)
                                    }
                                }
                                Connections {
                                    target: Room
                                    // Clear only after the server accepted the message.
                                    function onSent() { composer.clear(); chat.pinned = true }
                                }
                            }

                            ListView {
                                clip: true
                                model: Room.members
                                spacing: 6
                                delegate: RowLayout {
                                    required property var modelData
                                    width: ListView.view.width
                                    spacing: 12
                                    Rectangle {
                                        implicitWidth: 36; implicitHeight: 36; radius: 18
                                        color: Theme.c.selected
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.initials
                                            color: Theme.c.accent
                                            font.family: Theme.displayFamily
                                            font.pixelSize: 14
                                            font.weight: Font.Bold
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.name
                                        color: K.text
                                        font.family: Theme.fontFamily
                                        font.pixelSize: 14
                                        elide: Text.ElideRight
                                    }
                                    Rectangle {
                                        implicitHeight: 24
                                        implicitWidth: roleText.implicitWidth + 18
                                        radius: 12
                                        color: modelData.role === "member" ? K.glass : Qt.alpha(Theme.c.accent, 0.16)
                                        Text {
                                            id: roleText
                                            anchors.centerIn: parent
                                            text: modelData.role === "owner" ? I18n.s.roleOwner
                                                  : modelData.role === "moderator" ? I18n.s.roleModerator : I18n.s.roleMember
                                            color: modelData.role === "member" ? K.muted : Theme.c.accent
                                            font.family: Theme.fontFamily
                                            font.pixelSize: 11
                                            font.weight: Font.DemiBold
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
}
