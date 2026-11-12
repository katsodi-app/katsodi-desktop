import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// A shared room: header, stage (source) and the conversation panel with
// members. Data and permissions come from the server snapshot.
Item {
    id: screen
    property string roomId
    signal back()
    signal navigate(string name, string id)
    readonly property bool narrow: width < 980
    readonly property int side: width < 860 ? 16 : 44

    Component.onCompleted: Room.open(roomId)
    Component.onDestruction: Room.close()
    onRoomIdChanged: if (roomId) Room.open(roomId)

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: screen.side
        anchors.rightMargin: screen.side
        anchors.bottomMargin: 20
        spacing: 16

        // Header
        RowLayout {
            Layout.fillWidth: true
            spacing: 12
            KButton { kind: "ghost"; small: true; text: "← " + I18n.s.back; onClicked: screen.back() }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Text {
                    Layout.fillWidth: true
                    text: Room.name || (Room.loading ? I18n.s.loading : "")
                    color: Theme.c.foreground
                    font.pixelSize: 26
                    font.weight: Font.DemiBold
                    font.letterSpacing: -1.2
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                Text {
                    Layout.fillWidth: true
                    visible: Room.name !== ""
                    text: [Room.format === "music" ? I18n.s.roomMusic : I18n.s.roomVideo,
                           Room.visibility === "private" ? I18n.s.roomPrivate : I18n.s.roomPublic,
                           Room.fandomId ? Fandoms.nameOf(Room.fandomId) : I18n.s.roomOther].join(" · ")
                    color: Theme.c.mutedForeground
                    font.pixelSize: 12
                    elide: Text.ElideRight
                }
            }
            KButton {
                visible: Room.name !== "" && !Room.joined && Account.signedIn && !Room.closed
                text: Room.joining ? I18n.s.loading : I18n.s.roomJoin
                enabled: !Room.joining
                onClicked: Room.join()
            }
            KButton {
                visible: Room.name !== "" && !Account.signedIn
                kind: "secondary"
                text: I18n.s.signIn
                onClicked: screen.navigate("signin", "")
            }
        }

        // Room-level states
        Rectangle {
            Layout.fillWidth: true
            visible: Room.errorKey !== "" || Room.actionErrorKey !== ""
            implicitHeight: notice.implicitHeight + 24
            radius: 12
            color: Theme.c.surface
            border.width: 1
            border.color: Theme.c.border
            RowLayout {
                id: notice
                anchors.fill: parent
                anchors.margins: 12
                Text {
                    Layout.fillWidth: true
                    text: I18n.s[Room.errorKey || Room.actionErrorKey] || I18n.s.errorServer
                    color: Room.errorKey === "roomClosed" ? Theme.c.mutedForeground : Theme.c.danger
                    wrapMode: Text.WordWrap
                    font.pixelSize: 13
                }
                KButton {
                    visible: Room.errorKey !== "" && Room.errorKey !== "roomClosed"
                    kind: "secondary"; small: true
                    text: I18n.s.retry
                    onClicked: Room.retry()
                }
            }
        }

        // Keeps the header at the top while the room is loading or missing.
        Item { Layout.fillHeight: true; visible: Room.name === "" }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: Room.name !== ""
            columns: screen.narrow ? 1 : 2
            columnSpacing: 16
            rowSpacing: 16

            // Stage
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: !screen.narrow
                Layout.preferredHeight: screen.narrow ? 220 : -1
                Layout.minimumHeight: 200
                radius: 16
                color: Theme.c.surface
                border.width: 1
                border.color: Theme.c.border
                ColumnLayout {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 460)
                    spacing: 12
                    Text {
                        Layout.fillWidth: true
                        text: I18n.s.stageSource.toUpperCase()
                        color: Theme.c.mutedForeground
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        font.letterSpacing: 1.44
                        horizontalAlignment: Text.AlignHCenter
                    }
                    Text {
                        Layout.fillWidth: true
                        text: Room.source ? Links.host(Room.source) : I18n.s.stageNoSource
                        color: Theme.c.foreground
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideMiddle
                    }
                    Text {
                        Layout.fillWidth: true
                        visible: Room.source !== ""
                        text: I18n.s.stageProvider
                        color: Theme.c.mutedForeground
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                        horizontalAlignment: Text.AlignHCenter
                    }
                    KButton {
                        Layout.alignment: Qt.AlignHCenter
                        visible: Room.source !== ""
                        kind: "secondary"
                        small: true
                        text: I18n.s.stageOpenSource
                        onClicked: Links.openExternal(Room.source)
                    }
                }
            }

            // Conversation
            Rectangle {
                Layout.fillHeight: true
                Layout.fillWidth: screen.narrow
                Layout.preferredWidth: screen.narrow ? -1 : 360
                Layout.minimumHeight: 320
                radius: 16
                color: Theme.c.surface
                border.width: 1
                border.color: Theme.c.border

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 10

                    TabBar {
                        id: tabs
                        Layout.fillWidth: true
                        background: Item {}
                        Repeater {
                            model: [I18n.s.roomChat, I18n.s.roomMembers + " · " + Room.members.length]
                            delegate: TabButton {
                                id: tab
                                required property string modelData
                                required property int index
                                text: modelData
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                contentItem: Text {
                                    text: tab.text
                                    font: tab.font
                                    color: tabs.currentIndex === tab.index ? Theme.c.foreground : Theme.c.mutedForeground
                                    horizontalAlignment: Text.AlignHCenter
                                }
                                background: Rectangle {
                                    color: "transparent"
                                    Rectangle {
                                        anchors.bottom: parent.bottom
                                        width: parent.width; height: 2
                                        color: tabs.currentIndex === tab.index ? Theme.c.accent : Theme.c.border
                                    }
                                }
                            }
                        }
                    }

                    StackLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: tabs.currentIndex

                        // Chat
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
                                // Follow new messages only when already at the bottom.
                                property bool pinned: true
                                onMovementEnded: pinned = atYEnd
                                onCountChanged: if (pinned) Qt.callLater(positionViewAtEnd)
                                ScrollBar.vertical: ScrollBar {}
                                delegate: ColumnLayout {
                                    id: message
                                    required property string author
                                    required property string body
                                    required property string kind
                                    required property var time
                                    required property bool grouped
                                    required property bool own
                                    width: chat.width - 8
                                    spacing: 2
                                    RowLayout {
                                        visible: !message.grouped
                                        Layout.topMargin: 8
                                        spacing: 8
                                        Text {
                                            text: message.author
                                            color: message.own ? Theme.c.accent : Theme.c.foreground
                                            font.pixelSize: 12
                                            font.weight: Font.Bold
                                        }
                                        Text {
                                            text: Qt.formatTime(message.time, "HH:mm")
                                            color: Theme.c.mutedForeground
                                            font.pixelSize: 11
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        // Plain text only: no markup from other users is interpreted.
                                        textFormat: Text.PlainText
                                        text: message.kind === "sticker" ? "✦ " + I18n.s.stickerSent
                                              : message.kind === "image" ? "▣ " + I18n.s.imageMessage
                                              : message.body
                                        color: message.kind === "text" ? Theme.c.foreground : Theme.c.mutedForeground
                                        font.pixelSize: 14
                                        wrapMode: Text.Wrap
                                    }
                                }
                                Text {
                                    anchors.centerIn: parent
                                    width: parent.width - 32
                                    visible: chat.count === 0 && !Room.loading && Room.joined
                                    text: I18n.s.roomNoMessages
                                    color: Theme.c.mutedForeground
                                    horizontalAlignment: Text.AlignHCenter
                                    wrapMode: Text.WordWrap
                                }
                            }

                            // Composer
                            Text {
                                Layout.fillWidth: true
                                visible: !Room.joined
                                text: Account.signedIn ? I18n.s.roomJoin + " — " + I18n.s.roomChat : I18n.s.roomSignInToJoin
                                color: Theme.c.mutedForeground
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            RowLayout {
                                Layout.fillWidth: true
                                visible: Room.joined && !Room.closed
                                spacing: 8
                                ScrollView {
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.min(120, Math.max(44, composer.implicitHeight))
                                    TextArea {
                                        id: composer
                                        placeholderText: I18n.s.roomComposer
                                        placeholderTextColor: Theme.c.mutedForeground
                                        color: Theme.c.foreground
                                        font.pixelSize: 14
                                        wrapMode: TextArea.Wrap
                                        Accessible.name: I18n.s.roomComposer
                                        background: Rectangle {
                                            radius: 10
                                            color: Theme.c.surfaceElevated
                                            border.width: composer.activeFocus ? 2 : 1
                                            border.color: composer.activeFocus ? Theme.c.focusRing : Theme.c.border
                                        }
                                        // Enter sends; Shift+Enter adds a line. IME composition is untouched.
                                        Keys.onReturnPressed: (event) => {
                                            if (event.modifiers & Qt.ShiftModifier || composer.inputMethodComposing) {
                                                event.accepted = false
                                                return
                                            }
                                            Room.send(composer.text)
                                        }
                                    }
                                }
                                KButton {
                                    text: Room.sending ? I18n.s.roomSending : I18n.s.roomSend
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

                        // Members
                        ListView {
                            clip: true
                            model: Room.members
                            spacing: 6
                            delegate: RowLayout {
                                required property var modelData
                                width: ListView.view.width
                                spacing: 10
                                Rectangle {
                                    implicitWidth: 30; implicitHeight: 30; radius: 15
                                    color: Theme.c.selected
                                    Text {
                                        anchors.centerIn: parent
                                        text: modelData.initials
                                        color: Theme.c.accent
                                        font.pixelSize: 11
                                        font.weight: Font.Bold
                                    }
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: modelData.name
                                    color: Theme.c.foreground
                                    font.pixelSize: 13
                                    elide: Text.ElideRight
                                }
                                Text {
                                    text: modelData.role === "owner" ? I18n.s.roleOwner
                                          : modelData.role === "moderator" ? I18n.s.roleModerator : I18n.s.roleMember
                                    color: modelData.role === "member" ? Theme.c.mutedForeground : Theme.c.accent
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
