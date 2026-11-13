import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// Fandom poster card: lifts on hover, star toggles "my fandoms" (local).
AbstractButton {
    id: card
    property string fandomId
    property string name
    property string category
    property string art
    property string initials
    property int order: 0
    property int tile: 158
    implicitWidth: tile
    implicitHeight: tile * 1.42 + 52
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: name

    readonly property var labels: ({ cinema: I18n.s.catCinema, anime: I18n.s.catAnime, games: I18n.s.catGames,
                                     music: I18n.s.catMusic, books: I18n.s.catBooks, sport: I18n.s.catSport })

    opacity: 0
    transform: Translate { id: rise; y: K.still ? 0 : 16 }
    Component.onCompleted: reveal.start()
    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: K.still ? 0 : Math.min(card.order, 12) * 35 }
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: K.base }
            NumberAnimation { target: rise; property: "y"; to: 0; duration: K.slow; easing.type: Easing.OutCubic }
        }
    }

    onClicked: Library.toggleFollowed(fandomId)
    background: Item {}
    contentItem: Item {
        Poster {
            id: poster
            width: card.tile
            height: card.tile * 1.42
            radius: 16
            art: card.art
            initials: card.initials
            scale: card.hovered ? 1.03 : 1
            y: card.hovered ? -4 : 0
            Behavior on scale { NumberAnimation { duration: K.base; easing.type: K.easing } }
            Behavior on y { NumberAnimation { duration: K.base; easing.type: K.easing } }
            Rectangle {
                anchors.fill: parent
                radius: 16
                color: "transparent"
                border.width: card.visualFocus ? 2 : 0
                border.color: Theme.c.focusRing
            }
            IconButton {
                readonly property bool followed: Library.version >= 0 && Library.isFollowed(card.fandomId)
                visible: card.hovered || followed
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 8
                diameter: 34
                iconName: "star"
                active: followed
                tip: followed ? I18n.s.following : I18n.s.follow
                onClicked: Library.toggleFollowed(card.fandomId)
            }
        }
        Text {
            id: label
            anchors.top: poster.bottom
            anchors.topMargin: 10
            width: card.tile
            text: card.name
            color: K.text
            font.family: Theme.fontFamily
            font.pixelSize: 13
            font.weight: Font.Bold
            elide: Text.ElideRight
        }
        Text {
            anchors.top: label.bottom
            anchors.topMargin: 2
            width: card.tile
            text: card.labels[card.category] || ""
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 11
            elide: Text.ElideRight
        }
    }
}
