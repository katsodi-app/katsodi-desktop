import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Friends: ask by @username, answer requests, see friends, outgoing
// requests and blocked people. The server (the website's own actions)
// decides every outcome; this screen only shows them.
Item {
    id: page
    signal navigate(string name, string id)
    readonly property int side: width < 760 ? 20 : 40
    readonly property var outcomes: ({ sent: I18n.s.friendsOutcomeSent, friends: I18n.s.friendsOutcomeFriends,
                                       already: I18n.s.friendsOutcomeAlready, unavailable: I18n.s.friendsOutcomeUnavailable })
    property bool invalidName: false

    Component.onCompleted: if (Account.signedIn) People.reload()

    Scroller {
        id: scroller
        anchors.fill: parent
        contentHeight: column.implicitHeight + 80

        ColumnLayout {
            id: column
            x: page.side
            width: scroller.width - page.side * 2
            spacing: 26

            PageHero {
                Layout.fillWidth: true
                Layout.topMargin: 48
                badge: I18n.s.friendsEyebrow
                title: I18n.s.friendsDisplay
                accent: I18n.s.friendsAccent
                lead: I18n.s.friendsIntro
                size: Math.max(44, Math.min(88, page.width * 0.07))
            }

            Text {
                visible: !Account.signedIn
                text: I18n.s.friendsSignIn
                color: K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 15
            }

            // Ask by @username.
            ColumnLayout {
                Layout.fillWidth: true
                Layout.maximumWidth: 620
                visible: Account.signedIn
                spacing: 8
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    KTextField {
                        id: nameField
                        Layout.fillWidth: true
                        iconName: "friends"
                        placeholderText: I18n.s.friendsAddPlaceholder
                        maximumLength: 25
                        onAccepted: send()
                        onTextEdited: page.invalidName = false
                        function send() {
                            page.invalidName = !People.ask(text)
                            if (!page.invalidName)
                                nameField.clear()
                        }
                    }
                    KButton {
                        kind: "primary"
                        iconName: "plus"
                        text: People.busy ? I18n.s.loading : I18n.s.friendsAdd
                        enabled: !People.busy && nameField.text.trim().length > 0
                        onClicked: nameField.send()
                    }
                }
                Text {
                    Layout.fillWidth: true
                    visible: text !== ""
                    text: page.invalidName ? I18n.s.friendsInvalidName
                          : People.errorKey !== "" ? (I18n.s[People.errorKey] || I18n.s.errorServer)
                          : (page.outcomes[People.outcome] || "")
                    color: page.invalidName || People.errorKey !== "" ? Theme.c.danger
                           : People.outcome === "unavailable" ? Theme.c.warning : Theme.c.success
                    font.family: Theme.fontFamily
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            Section {
                title: I18n.s.friendsIncoming
                iconName: "clock"
                model: People.incoming
                actions: [{ label: I18n.s.friendsAccept, kind: "primary", run: (p) => People.answer(p.requestId, true) },
                          { label: I18n.s.friendsDecline, kind: "ghost", run: (p) => People.answer(p.requestId, false) }]
            }
            Section {
                title: I18n.s.friendsList
                iconName: "friends"
                model: People.friends
                always: Account.signedIn
                empty: People.loading ? I18n.s.loading : I18n.s.friendsEmpty
                actions: [{ label: I18n.s.friendsRemove, kind: "ghost", run: (p) => People.change(p.userId, "remove") },
                          { label: I18n.s.friendsBlock, kind: "quiet", run: (p) => People.change(p.userId, "block") }]
            }
            Section {
                title: I18n.s.friendsOutgoing
                iconName: "chevron"
                model: People.outgoing
                actions: [{ label: I18n.s.friendsWithdraw, kind: "ghost", run: (p) => People.withdraw(p.requestId) }]
            }
            Section {
                title: I18n.s.friendsBlocked
                iconName: "lock"
                model: People.blocked
                actions: [{ label: I18n.s.friendsUnblock, kind: "ghost", run: (p) => People.change(p.userId, "unblock") }]
            }
        }
    }

    // A titled list of people with per-person actions.
    component Section: ColumnLayout {
        id: section
        property string title
        property string iconName
        property var model: []
        property var actions: []
        property bool always: false
        property string empty: ""
        Layout.fillWidth: true
        visible: model.length > 0 || always
        spacing: 10
        SectionTitle {
            Layout.fillWidth: true
            iconName: section.iconName
            title: section.title + (section.model.length ? " · " + section.model.length : "")
        }
        Text {
            visible: section.model.length === 0 && section.empty !== ""
            text: section.empty
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
        Repeater {
            model: section.model
            delegate: Glass {
                id: row
                required property var modelData
                Layout.fillWidth: true
                implicitHeight: 66
                radius: 16
                hoverable: true
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 14
                    anchors.rightMargin: 12
                    spacing: 12
                    AbstractButton {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hoverEnabled: true
                        onClicked: page.navigate("profile", row.modelData.username)
                        Accessible.name: row.modelData.name
                        background: Item {}
                        contentItem: RowLayout {
                            spacing: 12
                            Rectangle {
                                implicitWidth: 42; implicitHeight: 42; radius: 21
                                color: Theme.c.selected
                                Text { anchors.centerIn: parent; text: row.modelData.initials || "K"; color: Theme.c.accent
                                       font.family: Theme.displayFamily; font.pixelSize: 17; font.weight: Font.Bold }
                            }
                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 1
                                Text {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                    color: K.text
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 15
                                    font.weight: Font.DemiBold
                                    elide: Text.ElideRight
                                }
                                Text {
                                    Layout.fillWidth: true
                                    text: "@" + row.modelData.username
                                          + (row.modelData.since ? "  ·  " + I18n.s.friendsSince.replace("{date}", row.modelData.since) : "")
                                    color: K.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                    Repeater {
                        model: section.actions
                        delegate: KButton {
                            required property var modelData
                            small: true
                            kind: modelData.kind
                            text: modelData.label
                            enabled: !People.busy
                            onClicked: modelData.run(row.modelData)
                        }
                    }
                }
            }
        }
    }
}
