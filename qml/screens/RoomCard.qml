import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Shared room card (website .network-room-card): art panel, format and
// visibility badges, title and description.
ItemDelegate {
    id: card
    property string roomId
    property string name
    property string description
    property string format
    property string visibility
    property string art
    property string fandomId
    implicitWidth: 222
    implicitHeight: 300
    hoverEnabled: true
    padding: 12
    Accessible.name: name
    Accessible.role: Accessible.Link

    // Room art motifs of the website, drawn natively.
    readonly property var artColors: ({
        orbit: [Theme.c.accent, Theme.c.info],
        wave: [Theme.c.info, Theme.c.success],
        moon: [Theme.c.warning, Theme.c.accent],
        sun: [Theme.c.danger, Theme.c.warning]
    })

    background: Rectangle {
        radius: 16
        color: card.hovered ? Theme.c.surfaceElevated : Theme.c.surface
        border.width: card.visualFocus ? 2 : 1
        border.color: card.visualFocus ? Theme.c.focusRing : Theme.c.border
    }
    contentItem: ColumnLayout {
        spacing: 10
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 150
            radius: 12
            color: Theme.c.selected
            clip: true
            readonly property var tones: card.artColors[card.art] || card.artColors.orbit
            Rectangle {
                width: parent.width * 0.62; height: width; radius: width / 2
                x: parent.width * 0.48; y: -height * 0.25
                color: Qt.alpha(parent.tones[0], 0.32)
            }
            Rectangle {
                width: parent.width * 0.42; height: width; radius: width / 2
                x: parent.width * 0.08; y: parent.height * 0.45
                color: Qt.alpha(parent.tones[1], 0.26)
            }
            Poster {
                visible: card.fandomId !== "" && Fandoms.nameOf(card.fandomId) !== ""
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: 12
                width: 52; height: 72; radius: 8
                cornerColor: Theme.c.selected
                source: visible ? Fandoms.artOf(card.fandomId) : ""
                initials: Fandoms.nameOf(card.fandomId).charAt(0)
            }
        }
        RowLayout {
            spacing: 6
            Repeater {
                model: [card.format === "music" ? I18n.s.roomMusic : I18n.s.roomVideo,
                        card.visibility === "private" ? I18n.s.roomPrivate : I18n.s.roomPublic]
                delegate: Rectangle {
                    required property string modelData
                    implicitHeight: 22
                    implicitWidth: badge.implicitWidth + 16
                    radius: 6
                    color: Theme.c.surfaceElevated
                    Text {
                        id: badge
                        anchors.centerIn: parent
                        text: parent.modelData
                        color: Theme.c.mutedForeground
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                    }
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: card.name
            color: Theme.c.foreground
            font.pixelSize: 19
            font.weight: 650
            font.letterSpacing: -0.76
            elide: Text.ElideRight
        }
        Text {
            Layout.fillWidth: true
            Layout.fillHeight: true
            text: card.description || (card.fandomId ? Fandoms.nameOf(card.fandomId) : I18n.s.roomOther)
            color: Theme.c.mutedForeground
            font.pixelSize: 12
            lineHeight: 1.4
            wrapMode: Text.WordWrap
            maximumLineCount: 2
            elide: Text.ElideRight
        }
    }
}
