import QtQuick
import QtQuick.Controls
import QtQuick.Shapes
import Katsodi.App
import Katsodi.Ui

// Room card in Liner's shelf style: a square art tile (the room's motif
// in its palette, the fandom poster and the format), title and subtitle
// below. Lifts on hover; the bookmark appears on hover for shared rooms.
AbstractButton {
    id: card
    property string roomId
    property string name
    property string description
    property string format
    property string visibility
    property string art: "orbit"
    property string fandomId
    property bool draft: false
    property int order: 0
    property int tile: 176
    signal removeRequested()
    implicitWidth: tile
    implicitHeight: tile + 58
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: name
    Accessible.role: Accessible.Link

    readonly property var tones: ({ orbit: [Theme.c.accent, Theme.c.info], wave: [Theme.c.info, Theme.c.success],
                                   moon: [Theme.c.warning, Theme.c.accent], sun: [Theme.c.danger, Theme.c.warning] })
    readonly property var tone: tones[art] || tones.orbit
    readonly property string fandomArt: fandomId ? Fandoms.artOf(fandomId) : ""

    // Staggered entrance.
    opacity: 0
    transform: Translate { id: rise; y: K.still ? 0 : 18 }
    Component.onCompleted: reveal.start()
    SequentialAnimation {
        id: reveal
        PauseAnimation { duration: K.still ? 0 : Math.min(card.order, 10) * 45 }
        ParallelAnimation {
            NumberAnimation { target: card; property: "opacity"; to: 1; duration: K.base }
            NumberAnimation { target: rise; property: "y"; to: 0; duration: K.slow; easing.type: Easing.OutCubic }
        }
    }

    background: Item {}
    contentItem: Item {
        Rectangle {
            id: artTile
            width: card.tile
            height: card.tile
            radius: K.rCard
            scale: card.hovered ? 1.03 : 1
            y: card.hovered ? -4 : 0
            Behavior on scale { NumberAnimation { duration: K.base; easing.type: K.easing } }
            Behavior on y { NumberAnimation { duration: K.base; easing.type: K.easing } }
            color: Theme.c.surfaceElevated
            border.width: card.visualFocus ? 2 : 1
            border.color: card.visualFocus ? Theme.c.focusRing : K.stroke

            // Two soft lights in the room's motif colours.
            Shape {
                anchors.fill: parent
                preferredRendererType: Shape.GeometryRenderer
                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: artTile.width * 0.78; centerY: artTile.height * 0.18
                        centerRadius: artTile.width * 0.75
                        focalX: centerX; focalY: centerY
                        GradientStop { position: 0; color: Qt.alpha(card.tone[0], 0.75) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                    PathRectangle { x: 0; y: 0; width: artTile.width; height: artTile.height; radius: K.rCard }
                }
                ShapePath {
                    strokeWidth: -1
                    fillGradient: RadialGradient {
                        centerX: artTile.width * 0.12; centerY: artTile.height * 0.95
                        centerRadius: artTile.width * 0.65
                        focalX: centerX; focalY: centerY
                        GradientStop { position: 0; color: Qt.alpha(card.tone[1], 0.6) }
                        GradientStop { position: 1; color: "transparent" }
                    }
                    PathRectangle { x: 0; y: 0; width: artTile.width; height: artTile.height; radius: K.rCard }
                }
            }
            Rectangle {
                x: 12; y: 12
                width: 34; height: 34; radius: 17
                color: Qt.alpha(Theme.c.background, 0.55)
                Icon { anchors.centerIn: parent; size: 17; name: card.format === "music" ? "music" : "video"; color: Theme.c.foreground }
            }
            Rectangle {
                visible: (card.draft || card.visibility === "private") && !card.hovered
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 12
                width: 34; height: 34; radius: 17
                color: Qt.alpha(Theme.c.background, 0.55)
                Icon { anchors.centerIn: parent; size: 16; name: card.draft ? "library" : "lock"; color: Theme.c.foreground }
            }
            Poster {
                visible: card.fandomArt !== ""
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                width: card.tile * 0.34
                height: width * 1.42
                radius: 10
                art: card.fandomArt
                initials: Fandoms.nameOf(card.fandomId).charAt(0)
            }
            Text {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: 14
                text: card.name.length ? card.name.charAt(0).toUpperCase() : "K"
                color: Theme.c.foreground
                font.family: Theme.displayFamily
                font.pixelSize: card.tile * 0.34
                font.weight: Font.Bold
                opacity: 0.92
            }
            IconButton {
                visible: card.hovered || activeFocus
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                diameter: 36
                iconName: card.draft ? "trash" : "bookmark"
                active: !card.draft && Library.version >= 0 && Library.isSaved(card.roomId)
                tip: card.draft ? I18n.s.remove : (Library.version >= 0 && Library.isSaved(card.roomId) ? I18n.s.roomUnsave : I18n.s.roomSave)
                onClicked: card.draft ? card.removeRequested() : Library.toggleSaved(card.roomId)
            }
        }

        Text {
            id: title
            anchors.top: artTile.bottom
            anchors.topMargin: 12
            width: card.tile
            text: card.name
            color: K.text
            font.family: Theme.fontFamily
            font.pixelSize: 14
            font.weight: Font.Bold
            elide: Text.ElideRight
        }
        Text {
            anchors.top: title.bottom
            anchors.topMargin: 3
            width: card.tile
            text: card.draft ? I18n.s.draftLocal
                  : (card.description || (card.fandomId ? Fandoms.nameOf(card.fandomId) : (card.format === "music" ? I18n.s.roomMusic : I18n.s.roomVideo)))
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            elide: Text.ElideRight
        }
    }
}
