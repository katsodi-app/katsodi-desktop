import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Quick search (Ctrl+K), as in Liner: a frosted panel over a dimmed app,
// category pills and keyboard navigation (↑ ↓ Enter, Esc closes).
Popup {
    id: overlay
    signal openRoom(string id)
    signal openFandom(string id)
    modal: true
    focus: true
    padding: 0
    x: (parent.width - width) / 2
    y: Math.min(90, parent.height * 0.08)
    width: Math.min(680, parent.width - 32)
    height: Math.min(560, parent.height - y - 24)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    onOpened: { field.text = Search.query; field.forceActiveFocus(); field.selectAll(); results.currentIndex = 0 }

    Overlay.modal: Rectangle {
        color: Qt.alpha(Theme.c.background, 0.8)
        Behavior on opacity { NumberAnimation { duration: K.base } }
    }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base }
            NumberAnimation { property: "scale"; from: K.still ? 1 : 0.96; to: 1; duration: K.base; easing.type: K.easing }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }

    function activate(index) {
        const item = results.itemAtIndex(index)
        if (!item)
            return
        if (item.type === "room")
            overlay.openRoom(item.itemId)
        else
            overlay.openFandom(item.itemId)
        overlay.close()
    }

    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 12
        KTextField {
            id: field
            Layout.fillWidth: true
            Layout.margins: 14
            Layout.bottomMargin: 0
            iconName: "search"
            placeholderText: I18n.s.searchPlaceholder
            maximumLength: 200
            onTextEdited: { Search.query = text; results.currentIndex = 0 }
            Keys.onDownPressed: results.incrementCurrentIndex()
            Keys.onUpPressed: results.decrementCurrentIndex()
            Keys.onReturnPressed: overlay.activate(results.currentIndex)
        }
        Segmented {
            Layout.leftMargin: 14
            model: [{ value: "all", label: I18n.s.searchAll },
                    { value: "rooms", label: I18n.s.searchTabRooms },
                    { value: "fandoms", label: I18n.s.searchTabFandoms }]
            current: Search.filter
            onSelected: (value) => Search.filter = value
        }
        Text {
            Layout.fillWidth: true
            Layout.margins: 18
            visible: Search.count === 0
            text: Search.query.trim().length ? I18n.s.searchEmpty : I18n.s.searchHint
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 14
            wrapMode: Text.WordWrap
        }
        ListView {
            id: results
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.bottomMargin: 10
            clip: true
            model: Search
            spacing: 2
            keyNavigationWraps: true
            highlightMoveDuration: K.fast
            highlight: Rectangle {
                radius: 14
                color: K.glassHover
                x: 8
                width: results.width - 16
            }
            delegate: AbstractButton {
                id: row
                required property int index
                required property string type
                required property string itemId
                required property string title
                required property string subtitle
                required property var art
                required property string initials
                width: results.width
                height: 62
                hoverEnabled: true
                onHoveredChanged: if (hovered) results.currentIndex = index
                onClicked: overlay.activate(index)
                Accessible.name: title
                background: Item {}
                contentItem: RowLayout {
                    spacing: 14
                    Item { implicitWidth: 6 }
                    Poster {
                        implicitWidth: 44; implicitHeight: 44
                        radius: 10
                        art: row.type === "fandom" ? (row.art || "") : ""
                        initials: row.initials
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 2
                        Text {
                            Layout.fillWidth: true
                            text: row.title
                            color: K.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                        Text {
                            Layout.fillWidth: true
                            text: row.type === "fandom"
                                  ? ({ cinema: I18n.s.catCinema, anime: I18n.s.catAnime, games: I18n.s.catGames,
                                       music: I18n.s.catMusic, books: I18n.s.catBooks, sport: I18n.s.catSport })[row.subtitle] || row.subtitle
                                  : (row.subtitle === "music" ? I18n.s.roomMusic : row.subtitle === "video" ? I18n.s.roomVideo : row.subtitle)
                            color: K.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 12
                            elide: Text.ElideRight
                        }
                    }
                    Rectangle {
                        implicitHeight: 24
                        implicitWidth: kind.implicitWidth + 18
                        radius: 12
                        color: K.glass
                        border.width: 1
                        border.color: K.stroke
                        Text {
                            id: kind
                            anchors.centerIn: parent
                            text: row.type === "room" ? I18n.s.searchTabRooms : I18n.s.searchTabFandoms
                            color: K.muted
                            font.family: Theme.fontFamily
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                        }
                    }
                    Item { implicitWidth: 10 }
                }
            }
        }
    }
}
