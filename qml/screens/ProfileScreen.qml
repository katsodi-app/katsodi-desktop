import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// A profile as the website shows it to you: name, @username, about, level
// and achievements, fandoms; befriend or block. Visible to yourself and to
// people you share an active room with.
Item {
    id: page
    property string username
    signal back()
    signal navigate(string name, string id)
    readonly property var p: People.profile
    readonly property int side: width < 760 ? 20 : 40

    Component.onCompleted: People.openProfile(username)
    onUsernameChanged: People.openProfile(username)

    Scroller {
        id: scroller
        anchors.fill: parent
        contentHeight: column.implicitHeight + 80

        ColumnLayout {
            id: column
            x: page.side
            y: 24
            width: scroller.width - page.side * 2
            spacing: 26

            IconButton { iconName: "back"; tip: I18n.s.back; onClicked: page.back() }

            Text {
                visible: !People.profileLoading && People.profileErrorKey !== ""
                Layout.fillWidth: true
                text: I18n.s.profileHidden
                color: K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 15
                wrapMode: Text.WordWrap
            }
            BusyIndicator { visible: People.profileLoading; running: visible && !K.still }

            // Header.
            RowLayout {
                Layout.fillWidth: true
                visible: page.p.id !== undefined
                spacing: 26
                Rectangle {
                    implicitWidth: page.width < 760 ? 96 : 132
                    implicitHeight: implicitWidth
                    radius: implicitWidth / 2
                    color: Theme.c.selected
                    border.width: 3
                    border.color: Theme.c.primary
                    Text { anchors.centerIn: parent; text: page.p.initials || "K"; color: Theme.c.accent
                           font.family: Theme.displayFamily; font.pixelSize: parent.width * 0.38; font.weight: Font.Bold }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        text: page.p.displayName || ""
                        color: K.text
                        font.family: Theme.displayFamily
                        font.pixelSize: page.width < 760 ? 38 : 56
                        font.weight: Font.Bold
                        elide: Text.ElideRight
                    }
                    Text {
                        text: "@" + (page.p.username || "")
                              + (page.p.createdAt ? "  ·  " + I18n.s.profileSince.replace("{date}", page.p.createdAt) : "")
                        color: K.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 14
                    }
                    RowLayout {
                        spacing: 10
                        // What you can do depends on how you two are connected.
                        KButton {
                            visible: page.p.relation === "none"
                            kind: "primary"; iconName: "plus"
                            text: I18n.s.profileAddFriend
                            enabled: !People.busy
                            onClicked: People.ask(page.p.username)
                        }
                        KButton {
                            visible: page.p.relation === "incoming"
                            kind: "primary"; iconName: "plus"
                            text: I18n.s.friendsAccept
                            enabled: !People.busy
                            onClicked: {
                                for (const r of People.incoming)
                                    if (r.userId === page.p.id) People.answer(r.requestId, true)
                            }
                        }
                        Rectangle {
                            visible: ["friend", "outgoing", "self"].indexOf(page.p.relation) >= 0
                            implicitHeight: 34
                            implicitWidth: state.implicitWidth + 28
                            radius: 17
                            color: K.glass
                            border.width: 1
                            border.color: K.stroke
                            Text {
                                id: state
                                anchors.centerIn: parent
                                text: page.p.relation === "friend" ? I18n.s.profileFriend
                                      : page.p.relation === "outgoing" ? I18n.s.profileRequested : I18n.s.profileYou
                                color: K.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                        KButton {
                            visible: page.p.relation !== "self" && page.p.relation !== undefined
                            kind: "quiet"; small: true
                            text: page.p.relation === "blocked" ? I18n.s.friendsUnblock : I18n.s.friendsBlock
                            enabled: !People.busy
                            onClicked: People.change(page.p.id, page.p.relation === "blocked" ? "unblock" : "block")
                        }
                    }
                }
            }

            // Level.
            Glass {
                Layout.fillWidth: true
                visible: page.p.level !== undefined
                implicitHeight: level.implicitHeight + 40
                radius: K.rPanel
                ColumnLayout {
                    id: level
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 20
                    spacing: 10
                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: I18n.s.profileLevel.replace("{n}", page.p.level || 1)
                            color: K.text
                            font.family: Theme.displayFamily
                            font.pixelSize: 28
                            font.weight: Font.Bold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: I18n.s.profileAchievements.replace("{u}", page.p.unlocked || 0).replace("{n}", page.p.achievements || 0)
                            color: K.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 13
                        }
                    }
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 8
                        radius: 4
                        color: K.stroke
                        Rectangle {
                            width: parent.width * Math.min(1, (page.p.intoLevel || 0) / Math.max(1, page.p.nextLevel || 1))
                            height: parent.height
                            radius: 4
                            color: Theme.c.primary
                            Behavior on width { NumberAnimation { duration: K.slow; easing.type: K.easing } }
                        }
                    }
                    Text {
                        text: I18n.s.profileXp.replace("{into}", page.p.intoLevel || 0).replace("{next}", page.p.nextLevel || 1)
                        color: K.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 12
                    }
                }
            }

            // About.
            ColumnLayout {
                Layout.fillWidth: true
                visible: page.p.id !== undefined
                spacing: 8
                SectionTitle { Layout.fillWidth: true; iconName: "library"; title: I18n.s.profileAbout }
                Text {
                    Layout.fillWidth: true
                    text: page.p.bio || I18n.s.profileNoBio
                    color: page.p.bio ? K.text : K.muted
                    font.family: Theme.fontFamily
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                }
            }

            // Fandoms.
            ColumnLayout {
                Layout.fillWidth: true
                visible: (page.p.fandoms || []).length > 0
                spacing: 12
                SectionTitle { Layout.fillWidth: true; iconName: "fandoms"; title: I18n.s.profileFandoms }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: page.p.fandoms || []
                        delegate: AbstractButton {
                            id: chip
                            required property string modelData
                            readonly property string label: {
                                const known = Fandoms.nameOf(modelData)
                                if (known)
                                    return known
                                for (const c of (page.p.communityFandoms || []))
                                    if (c.id === modelData) return c.name
                                return ""
                            }
                            visible: label !== ""
                            hoverEnabled: true
                            onClicked: if (Fandoms.nameOf(modelData)) page.navigate("fandom", modelData)
                            background: Rectangle {
                                radius: 16
                                color: chip.hovered ? K.glassHover : K.glass
                                border.width: 1
                                border.color: K.stroke
                            }
                            contentItem: Text {
                                leftPadding: 14; rightPadding: 14; topPadding: 7; bottomPadding: 7
                                text: chip.label
                                color: K.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                        }
                    }
                }
            }
        }
    }
}
