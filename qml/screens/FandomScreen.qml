import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// One fandom: poster, category, "follow", the website's Wikipedia excerpt
// with its attribution (CC BY-SA 4.0, link to the article), and the open
// rooms of this fandom.
Item {
    id: page
    property string fandomId
    signal back()
    signal navigate(string name, string id)
    signal createRequested()

    readonly property var fandom: Fandoms.info(fandomId, Prefs.locale)
    readonly property var labels: ({ cinema: I18n.s.catCinema, anime: I18n.s.catAnime, games: I18n.s.catGames,
                                     music: I18n.s.catMusic, books: I18n.s.catBooks, sport: I18n.s.catSport })
    readonly property var rooms: { Rooms.total; return Rooms.roomsOfFandom(fandomId) }
    readonly property bool followed: Library.version >= 0 && Library.isFollowed(fandomId)
    readonly property int side: width < 760 ? 20 : 40

    Component.onCompleted: if (Rooms.total === 0) Rooms.reload()

    Scroller {
        id: scroller
        anchors.fill: parent
        contentHeight: column.implicitHeight + 60

        ColumnLayout {
            id: column
            x: page.side
            y: 24
            width: scroller.width - page.side * 2
            spacing: 28

            RowLayout {
                Layout.fillWidth: true
                spacing: 14
                IconButton { iconName: "back"; tip: I18n.s.back; onClicked: page.back() }
                Item { Layout.fillWidth: true }
            }

            // Hero: poster and the essentials.
            RowLayout {
                Layout.fillWidth: true
                spacing: 32
                Poster {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: page.width < 760 ? 140 : 210
                    implicitHeight: implicitWidth * 1.45
                    radius: 18
                    framed: true
                    art: page.fandom.art || ""
                    initials: page.fandom.initials || "K"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    spacing: 14
                    Rectangle {
                        implicitHeight: 30
                        implicitWidth: category.implicitWidth + 26
                        radius: 15
                        color: K.glass
                        border.width: 1
                        border.color: K.stroke
                        Text {
                            id: category
                            anchors.centerIn: parent
                            text: page.labels[page.fandom.category] || ""
                            color: K.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                            font.weight: Font.Bold
                        }
                    }
                    Text {
                        Layout.fillWidth: true
                        text: page.fandom.name || ""
                        color: K.text
                        font.family: Theme.displayFamily
                        font.pixelSize: page.width < 760 ? 40 : 64
                        font.weight: Font.Bold
                        wrapMode: Text.WordWrap
                        lineHeight: 0.95
                    }
                    RowLayout {
                        spacing: 10
                        KButton {
                            kind: page.followed ? "ghost" : "primary"
                            iconName: "star"
                            text: page.followed ? I18n.s.following : I18n.s.follow
                            onClicked: Library.toggleFollowed(page.fandomId)
                        }
                        KButton {
                            kind: "ghost"
                            iconName: "plus"
                            text: I18n.s.fandomCreateRoom
                            onClicked: page.createRequested()
                        }
                    }
                }
            }

            // About, quoted from Wikipedia with its licence and source.
            Glass {
                Layout.fillWidth: true
                implicitHeight: about.implicitHeight + 48
                radius: K.rPanel
                ColumnLayout {
                    id: about
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 24
                    spacing: 12
                    SectionTitle { Layout.fillWidth: true; iconName: "library"; title: I18n.s.fandomAbout }
                    Text {
                        Layout.fillWidth: true
                        text: page.fandom.summary || I18n.s.fandomNoInfo
                        color: page.fandom.summary ? K.text : K.muted
                        font.family: Theme.fontFamily
                        font.pixelSize: 15
                        lineHeight: 1.25
                        wrapMode: Text.WordWrap
                    }
                    AbstractButton {
                        visible: (page.fandom.source || "") !== ""
                        hoverEnabled: true
                        onClicked: Links.openExternal(page.fandom.source)
                        Accessible.name: I18n.s.fandomSource
                        background: Item {}
                        contentItem: Row {
                            spacing: 6
                            Icon { anchors.verticalCenter: parent.verticalCenter; name: "link"; size: 14; color: K.muted }
                            Text {
                                text: I18n.s.fandomSource + (page.fandom.checked ? " · " + page.fandom.checked : "")
                                color: parent.parent.hovered ? K.text : K.muted
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                font.underline: parent.parent.hovered
                            }
                        }
                    }
                }
            }

            SectionTitle {
                Layout.fillWidth: true
                iconName: "rooms"
                title: I18n.s.fandomRooms + (page.rooms.length ? " · " + page.rooms.length : "")
            }
            Text {
                visible: page.rooms.length === 0
                text: I18n.s.fandomNoRooms
                color: K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 14
            }
            Flow {
                Layout.fillWidth: true
                visible: page.rooms.length > 0
                spacing: 18
                Repeater {
                    model: page.rooms
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
                        fandomId: modelData.fandomId
                        onClicked: page.navigate("room", roomId)
                    }
                }
            }
        }
    }
}
