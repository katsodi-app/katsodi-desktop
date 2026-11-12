import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Home: greeting, a row of fandoms from the bundled catalogue and the
// rooms open right now on the server.
ScrollView {
    id: screen
    signal navigate(string name, string id)
    contentWidth: availableWidth
    clip: true

    ColumnLayout {
        width: screen.availableWidth
        spacing: 28

        Item { implicitHeight: 2 }

        // Heading row as on the website: large question left, intro right.
        RowLayout {
            Layout.fillWidth: true
            Layout.leftMargin: screen.width < 860 ? 16 : 44
            Layout.rightMargin: Layout.leftMargin
            spacing: 32
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12
                RowLayout {
                    spacing: 10
                    Rectangle { width: 22; height: 2; radius: 1; color: Theme.c.accent }
                    Text {
                        text: I18n.s.homeEyebrow.toUpperCase()
                        color: Theme.c.mutedForeground
                        font.pixelSize: 9
                        font.weight: Font.Bold
                        font.letterSpacing: 1.44
                    }
                }
                Text {
                    Layout.fillWidth: true
                    text: I18n.s.homeTitle
                    color: Theme.c.foreground
                    font.pixelSize: Math.max(32, Math.min(60, screen.width * 0.05))
                    font.weight: Font.DemiBold
                    font.letterSpacing: -font.pixelSize * 0.07
                    wrapMode: Text.WordWrap
                    Accessible.role: Accessible.Heading
                }
            }
            Text {
                visible: screen.width >= 860
                Layout.preferredWidth: 260
                Layout.alignment: Qt.AlignBottom
                text: I18n.s.homeIntro
                color: Theme.c.mutedForeground
                font.pixelSize: 14
                lineHeight: 1.45
                wrapMode: Text.WordWrap
            }
        }

        // Fandoms
        SectionHeader {
            title: I18n.s.homeFandoms
            action: I18n.s.homeFandomsAll
            onActivated: screen.navigate("fandoms", "")
        }
        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: 250
            Layout.leftMargin: screen.width < 860 ? 16 : 44
            orientation: ListView.Horizontal
            spacing: 14
            clip: true
            model: Fandoms
            cacheBuffer: 400
            boundsBehavior: Flickable.StopAtBounds
            delegate: ItemDelegate {
                id: fandomCard
                required property string name
                required property url art
                required property string initials
                width: 150
                height: 250
                padding: 0
                hoverEnabled: true
                Accessible.name: name
                onClicked: screen.navigate("fandoms", "")
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 10
                    Poster {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 210
                        radius: 14
                        source: fandomCard.art
                        initials: fandomCard.initials
                        border.width: fandomCard.hovered || fandomCard.visualFocus ? 2 : 0
                        border.color: fandomCard.visualFocus ? Theme.c.focusRing : Theme.c.accent
                    }
                    Text {
                        Layout.fillWidth: true
                        text: fandomCard.name
                        color: Theme.c.foreground
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }
                }
            }
        }

        // Rooms open now
        SectionHeader {
            title: I18n.s.homeRooms
            action: I18n.s.homeRoomsAll
            onActivated: screen.navigate("rooms", "")
        }
        Text {
            Layout.leftMargin: screen.width < 860 ? 16 : 44
            visible: !Rooms.loading && Rooms.total === 0
            text: Rooms.errorKey ? I18n.s[Rooms.errorKey] || I18n.s.errorServer : I18n.s.roomsEmpty
            color: Theme.c.mutedForeground
            font.pixelSize: 14
        }
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: screen.width < 860 ? 16 : 44
            Layout.rightMargin: Layout.leftMargin
            spacing: 16
            Repeater {
                model: Rooms
                delegate: RoomCard {
                    required property var model
                    required property int index
                    roomId: model.roomId
                    name: model.name
                    description: model.description
                    format: model.format
                    visibility: model.visibility
                    art: model.art
                    fandomId: model.fandomId
                    visible: index < 4
                    width: visible ? implicitWidth : 0
                    onClicked: screen.navigate("room", roomId)
                }
            }
        }

        Item { implicitHeight: 40 }
    }

    component SectionHeader: RowLayout {
        id: header
        property string title
        property string action
        signal activated()
        Layout.fillWidth: true
        Layout.leftMargin: screen.width < 860 ? 16 : 44
        Layout.rightMargin: Layout.leftMargin
        Text {
            Layout.fillWidth: true
            text: header.title
            color: Theme.c.foreground
            font.pixelSize: 22
            font.weight: Font.DemiBold
            font.letterSpacing: -0.9
            Accessible.role: Accessible.Heading
        }
        KButton {
            kind: "ghost"
            small: true
            text: header.action + "  →"
            onClicked: header.activated()
        }
    }
}
