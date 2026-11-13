import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Home: the overview's hero over the night sky, then Liner shelves:
// rooms open now, fandoms, drafts and saved rooms from this device.
Scroller {
    id: page
    signal navigate(string name, string id)
    signal createRequested()
    readonly property int side: width < 760 ? 20 : 48
    contentHeight: column.implicitHeight + 120

    ColumnLayout {
        id: column
        width: page.width
        spacing: 34

        PageHero {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.topMargin: Math.max(40, page.height * 0.12)
            Layout.bottomMargin: 18
            badge: I18n.s.homeBadge
            title: I18n.s.homeDisplay
            accent: I18n.s.homeAccent
            lead: I18n.s.homeIntro
            size: Math.max(48, Math.min(104, page.width * 0.085))
            KButton { text: I18n.s.homeOpenRooms; iconName: "rooms"; onClicked: page.navigate("rooms", "") }
            KButton { kind: "ghost"; text: I18n.s.homeCreate; iconName: "plus"; onClicked: page.createRequested() }
        }

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            iconName: "flame"
            title: I18n.s.homeRooms
            action: I18n.s.homeRoomsAll
            onActivated: page.navigate("rooms", "")
        }
        Text {
            Layout.leftMargin: page.side
            visible: !Rooms.loading && Rooms.total === 0
            text: Rooms.errorKey ? (I18n.s[Rooms.errorKey] || I18n.s.errorServer) : I18n.s.roomsEmpty
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
        }
        HRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            Layout.leftMargin: page.side
            visible: Rooms.total > 0
            page: page
            model: Rooms
            delegate: RoomCard {
                required property var model
                required property int index
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

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            iconName: "fandoms"
            title: I18n.s.homeFandoms
            action: I18n.s.homeFandomsAll
            onActivated: page.navigate("fandoms", "")
        }
        HRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 290
            Layout.leftMargin: page.side
            page: page
            model: Fandoms
            delegate: FandomCard {
                onOpenRequested: (id) => page.navigate("fandom", id)
                required property var model
                required property int index
                order: index
                fandomId: model.fandomId
                name: model.name
                category: model.category
                art: model.art || ""
                initials: model.initials
            }
        }

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            visible: Library.count > 0
            iconName: "library"
            title: I18n.s.homeDrafts
            action: I18n.s.navLibrary
            onActivated: page.navigate("library", "")
        }
        HRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            Layout.leftMargin: page.side
            visible: Library.count > 0
            page: page
            model: Library
            delegate: RoomCard {
                required property var model
                required property int index
                order: index
                draft: true
                roomId: model.draftId
                name: model.name
                description: model.description
                format: model.format
                visibility: model.visibility
                art: model.art
                fandomId: model.fandomId || ""
                onRemoveRequested: Library.deleteDraft(roomId)
            }
        }
    }
}
