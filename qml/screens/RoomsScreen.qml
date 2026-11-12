import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Shared rooms catalogue: page intro, format chips, search and cards.
ScrollView {
    id: screen
    signal navigate(string name, string id)
    contentWidth: availableWidth
    clip: true
    readonly property int side: width < 860 ? 16 : 44

    Component.onCompleted: Rooms.reload()

    ColumnLayout {
        width: screen.availableWidth
        spacing: 24

        PageIntro {
            Layout.fillWidth: true
            Layout.leftMargin: screen.side
            Layout.rightMargin: screen.side
            Layout.topMargin: 4
            eyebrow: I18n.s.roomsEyebrow
            title: I18n.s.roomsTitle
            lead: I18n.s.roomsIntro
            KButton {
                visible: !Account.signedIn
                text: I18n.s.signIn
                onClicked: screen.navigate("signin", "")
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: screen.side
            Layout.rightMargin: screen.side
            spacing: 8
            Repeater {
                model: [{ value: "", label: I18n.s.filterAll },
                        { value: "video", label: I18n.s.filterVideo },
                        { value: "music", label: I18n.s.filterMusic }]
                delegate: KChip {
                    required property var modelData
                    text: modelData.label
                    selected: Rooms.format === modelData.value
                    onClicked: Rooms.format = modelData.value
                }
            }
            Item { Layout.fillWidth: true }
            KTextField {
                Layout.preferredWidth: Math.min(320, screen.width * 0.4)
                placeholderText: I18n.s.searchRooms
                text: Rooms.query
                onTextEdited: Rooms.query = text
            }
        }

        // States
        Text {
            Layout.leftMargin: screen.side
            visible: Rooms.loading && Rooms.total === 0
            text: I18n.s.loading
            color: Theme.c.mutedForeground
        }
        RowLayout {
            Layout.leftMargin: screen.side
            visible: !Rooms.loading && Rooms.errorKey !== ""
            spacing: 12
            Text {
                text: I18n.s[Rooms.errorKey] || I18n.s.errorServer
                color: Theme.c.danger
                font.pixelSize: 14
            }
            KButton { kind: "secondary"; small: true; text: I18n.s.retry; onClicked: Rooms.reload() }
        }
        Text {
            Layout.leftMargin: screen.side
            visible: !Rooms.loading && !Rooms.errorKey && grid.count === 0
            text: Rooms.total === 0 ? I18n.s.roomsEmpty : I18n.s.roomsNoMatch
            color: Theme.c.mutedForeground
            font.pixelSize: 14
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.leftMargin: screen.side
            Layout.rightMargin: screen.side
            Layout.preferredHeight: Math.ceil(count / Math.max(1, Math.floor(width / cellWidth))) * cellHeight
            interactive: false
            cellWidth: 238
            cellHeight: 316
            model: Rooms
            delegate: RoomCard {
                required property var model

                roomId: model.roomId
                name: model.name
                description: model.description
                format: model.format
                visibility: model.visibility
                art: model.art
                fandomId: model.fandomId
                width: 222
                onClicked: screen.navigate("room", roomId)
            }
        }
        Item { implicitHeight: 32 }
    }
}
