import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Fandom catalogue: headline, category pills, search, poster grid.
Scroller {
    id: page
    signal navigate(string name, string id)
    readonly property int side: width < 760 ? 20 : 48
    contentHeight: column.implicitHeight + 120
    Component.onDestruction: { Fandoms.category = ""; Fandoms.query = "" }

    ColumnLayout {
        id: column
        width: page.width
        spacing: 26

        PageHero {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.topMargin: 48
            title: I18n.s.fandomsDisplay
            accent: I18n.s.fandomsAccent
            lead: I18n.s.fandomsIntro
            size: Math.max(44, Math.min(88, page.width * 0.07))
        }

        // Category pills scroll sideways on narrow windows.
        Flickable {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.preferredHeight: 42
            contentWidth: pills.implicitWidth
            boundsBehavior: Flickable.StopAtBounds
            clip: true
            Segmented {
                id: pills
                model: [{ value: "", label: I18n.s.filterAll },
                        { value: "cinema", label: I18n.s.catCinema },
                        { value: "anime", label: I18n.s.catAnime },
                        { value: "games", label: I18n.s.catGames },
                        { value: "music", label: I18n.s.catMusic },
                        { value: "books", label: I18n.s.catBooks },
                        { value: "sport", label: I18n.s.catSport }]
                current: Fandoms.category
                onSelected: (value) => Fandoms.category = value
            }
        }
        KTextField {
            Layout.leftMargin: page.side
            Layout.preferredWidth: Math.min(340, page.width - 2 * page.side)
            iconName: "search"
            placeholderText: I18n.s.searchFandoms
            text: Fandoms.query
            onTextEdited: Fandoms.query = text
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            readonly property int columns: Math.max(2, Math.floor((width + 18) / 176))
            cellWidth: Math.floor(width / columns)
            cellHeight: (cellWidth - 18) * 1.42 + 70
            Layout.preferredHeight: Math.ceil(count / columns) * cellHeight
            interactive: false
            model: Fandoms
            delegate: Item {
                required property var model
                required property int index
                width: grid.cellWidth
                height: grid.cellHeight
                FandomCard {
                    onOpenRequested: (id) => page.navigate("fandom", id)
                    tile: grid.cellWidth - 18
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
}
