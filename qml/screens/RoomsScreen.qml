import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Shared rooms: headline, format pills, search and a responsive grid.
Scroller {
    id: page
    signal navigate(string name, string id)
    signal createRequested()
    readonly property int side: width < 760 ? 20 : 48
    contentHeight: column.implicitHeight + 120
    Component.onCompleted: Rooms.reload()

    ColumnLayout {
        id: column
        width: page.width
        spacing: 26

        PageHero {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.topMargin: 48
            title: I18n.s.roomsDisplay
            accent: I18n.s.roomsAccent
            lead: I18n.s.roomsIntro
            size: Math.max(44, Math.min(88, page.width * 0.07))
            KButton { kind: "accent"; text: I18n.s.homeCreate; iconName: "plus"; onClicked: page.createRequested() }
            KButton { visible: !Account.signedIn; kind: "ghost"; text: I18n.s.signIn; onClicked: page.navigate("signin", "") }
        }

        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            spacing: 12
            Segmented {
                model: [{ value: "", label: I18n.s.filterAll },
                        { value: "video", label: I18n.s.filterVideo },
                        { value: "music", label: I18n.s.filterMusic }]
                current: Rooms.format
                onSelected: (value) => Rooms.format = value
            }
            KTextField {
                width: Math.min(340, page.width - 2 * page.side)
                iconName: "search"
                placeholderText: I18n.s.searchRooms
                text: Rooms.query
                onTextEdited: Rooms.query = text
            }
        }

        // An invitation link or code from a friend (website or app).
        Flow {
            visible: Account.signedIn
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            spacing: 10
            KTextField {
                id: invitation
                width: Math.min(420, page.width - 2 * page.side)
                iconName: "link"
                placeholderText: I18n.s.inviteJoinPlaceholder
                Accessible.name: I18n.s.inviteJoinTitle
                property bool bad: false
                onTextEdited: bad = false
                onAccepted: join.clicked()
            }
            KButton {
                id: join
                kind: "ghost"
                text: I18n.s.inviteJoin
                enabled: invitation.text.trim().length > 0 && !Room.moderating
                onClicked: invitation.bad = !Room.redeemInvite(invitation.text)
            }
            Text {
                visible: invitation.bad || (Room.actionErrorKey !== "" && Room.roomId === "")
                width: Math.min(420, page.width - 2 * page.side)
                text: invitation.bad ? I18n.s.inviteBad : (I18n.s[Room.actionErrorKey] || I18n.s.errorServer)
                color: Theme.c.danger
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
        Connections {
            target: Room
            function onInvitationAccepted(roomId) {
                invitation.clear()
                page.navigate("room", roomId)
            }
        }

        Text {
            Layout.leftMargin: page.side
            visible: Rooms.loading && Rooms.total === 0
            text: I18n.s.loading
            color: K.muted
            font.family: Theme.fontFamily
        }
        RowLayout {
            Layout.leftMargin: page.side
            visible: !Rooms.loading && Rooms.errorKey !== ""
            spacing: 12
            Text { text: I18n.s[Rooms.errorKey] || I18n.s.errorServer; color: Theme.c.danger; font.family: Theme.fontFamily }
            KButton { kind: "ghost"; small: true; text: I18n.s.retry; onClicked: Rooms.reload() }
        }
        Text {
            Layout.leftMargin: page.side
            visible: !Rooms.loading && !Rooms.errorKey && grid.count === 0
            text: Rooms.total === 0 ? I18n.s.roomsEmpty : I18n.s.roomsNoMatch
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            readonly property int columns: Math.max(1, Math.floor((width + 20) / 196))
            cellWidth: Math.floor(width / columns)
            cellHeight: cellWidth - 20 + 76
            Layout.preferredHeight: Math.ceil(count / columns) * cellHeight
            interactive: false
            model: Rooms
            delegate: Item {
                required property var model
                required property int index
                width: grid.cellWidth
                height: grid.cellHeight
                RoomCard {
                    tile: grid.cellWidth - 20
                    order: index
                    roomId: model.roomId
                    name: model.name
                    description: model.description
                    format: model.format
                    visibility: model.visibility
                    art: model.art
                    fandomId: model.fandomId
                    onClicked: page.navigate("room", roomId)
                }
            }
        }
    }
}
