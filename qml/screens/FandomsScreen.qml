import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Fandom catalogue: category chips, search, poster grid (bundled data).
Item {
    id: screen
    readonly property int side: width < 860 ? 16 : 44
    Component.onDestruction: { Fandoms.category = ""; Fandoms.query = "" }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: screen.side
        anchors.rightMargin: screen.side
        spacing: 20

        PageIntro {
            Layout.fillWidth: true
            Layout.topMargin: 4
            eyebrow: I18n.s.fandomsEyebrow
            title: I18n.s.fandomsTitle
            lead: I18n.s.fandomsIntro
        }

        Flow {
            Layout.fillWidth: true
            spacing: 8
            Repeater {
                model: [{ value: "", label: I18n.s.filterAll },
                        { value: "cinema", label: I18n.s.catCinema },
                        { value: "anime", label: I18n.s.catAnime },
                        { value: "games", label: I18n.s.catGames },
                        { value: "music", label: I18n.s.catMusic },
                        { value: "books", label: I18n.s.catBooks },
                        { value: "sport", label: I18n.s.catSport }]
                delegate: KChip {
                    required property var modelData
                    text: modelData.label
                    selected: Fandoms.category === modelData.value
                    onClicked: Fandoms.category = modelData.value
                }
            }
            KTextField {
                width: Math.min(280, screen.width - 2 * screen.side)
                placeholderText: I18n.s.searchFandoms
                text: Fandoms.query
                onTextEdited: Fandoms.query = text
            }
        }

        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: Fandoms
            cellWidth: 166
            cellHeight: 262
            cacheBuffer: 300
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: ColumnLayout {
                id: card
                required property string name
                required property url art
                required property string initials
                width: 150
                spacing: 8
                Poster {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 214
                    radius: 14
                    source: card.art
                    initials: card.initials
                }
                Text {
                    Layout.fillWidth: true
                    text: card.name
                    color: Theme.c.foreground
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }
        }
    }
}
