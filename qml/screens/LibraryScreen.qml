import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// What belongs to this device: drafts, saved shared rooms, my fandoms.
Scroller {
    id: page
    signal navigate(string name, string id)
    signal createRequested()
    readonly property int side: width < 760 ? 20 : 48
    contentHeight: column.implicitHeight + 120

    // Saved rooms that are currently open on the server.
    readonly property var savedRooms: {
        Library.version; Rooms.total
        return Library.savedIds().map((id) => Rooms.room(id)).filter((room) => room.roomId !== undefined)
    }

    ColumnLayout {
        id: column
        width: page.width
        spacing: 26

        PageHero {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.topMargin: 48
            title: I18n.s.libraryDisplay
            accent: I18n.s.libraryAccent
            lead: I18n.s.libraryIntro
            size: Math.max(44, Math.min(88, page.width * 0.07))
            KButton { kind: "accent"; text: I18n.s.homeCreate; iconName: "plus"; onClicked: page.createRequested() }
        }

        Glass {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            visible: Library.recovered || !Library.available
            implicitHeight: notice.implicitHeight + 28
            Text {
                id: notice
                anchors.fill: parent
                anchors.margins: 14
                text: Library.available ? I18n.s.libraryRecovered : I18n.s.libraryUnavailable
                color: Theme.c.warning
                font.family: Theme.fontFamily
                wrapMode: Text.WordWrap
            }
        }

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            iconName: "library"
            title: I18n.s.draftsTitle
        }
        Text {
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.fillWidth: true
            visible: Library.count === 0
            text: I18n.s.draftsEmpty
            color: K.muted
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
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

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            iconName: "bookmark"
            title: I18n.s.savedTitle
        }
        Text {
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.fillWidth: true
            visible: page.savedRooms.length === 0
            text: I18n.s.savedEmpty
            color: K.muted
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        HRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 240
            Layout.leftMargin: page.side
            visible: page.savedRooms.length > 0
            page: page
            model: page.savedRooms
            delegate: RoomCard {
                required property var modelData
                required property int index
                order: index
                roomId: modelData.roomId
                name: modelData.name
                description: modelData.description
                format: modelData.format
                visibility: modelData.visibility
                art: modelData.art
                fandomId: modelData.fandomId || ""
                onClicked: page.navigate("room", roomId)
            }
        }

        SectionTitle {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            iconName: "star"
            title: I18n.s.followedTitle
        }
        Text {
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.fillWidth: true
            visible: Library.version >= 0 && Library.followedCount() === 0
            text: I18n.s.followedEmpty
            color: K.muted
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
        HRow {
            Layout.fillWidth: true
            Layout.preferredHeight: 290
            Layout.leftMargin: page.side
            visible: Library.version >= 0 && Library.followedCount() > 0
            page: page
            model: Fandoms
            delegate: FandomCard {
                onOpenRequested: (id) => page.navigate("fandom", id)
                required property var model
                required property int index
                // Only followed fandoms take space in the shelf.
                readonly property bool shown: Library.version >= 0 && Library.isFollowed(model.fandomId)
                visible: shown
                width: shown ? implicitWidth : -18
                order: index
                fandomId: model.fandomId
                name: model.name
                category: model.category
                art: model.art || ""
                initials: model.initials
            }
        }
    }
}
