import QtQuick
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Overview headline: condensed Oswald words with the last word in the
// accent colour, a pill badge above and a lead below.
ColumnLayout {
    id: hero
    property string badge
    property string title
    property string accent
    property string lead
    property int size: 72
    default property alias actions: actionRow.data
    spacing: 18

    // Badge pill with a breathing dot (still when motion is reduced).
    Rectangle {
        visible: hero.badge.length > 0
        implicitHeight: 32
        implicitWidth: badgeRow.implicitWidth + 28
        radius: 16
        color: K.glass
        border.width: 1
        border.color: K.stroke
        Row {
            id: badgeRow
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                width: 8; height: 8; radius: 4
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.c.accent
                SequentialAnimation on opacity {
                    running: !K.still
                    loops: 3 // breathes a few times, then rests (idle windows do not redraw)
                    NumberAnimation { to: 0.35; duration: 1200; easing.type: Easing.InOutSine }
                    NumberAnimation { to: 1; duration: 1200; easing.type: Easing.InOutSine }
                }
            }
            Text {
                text: hero.badge
                color: K.text
                font.family: Theme.fontFamily
                font.pixelSize: 12
                font.weight: Font.Medium
            }
        }
    }

    // Title: Oswald, tight, with the accent word on the same line.
    Flow {
        Layout.fillWidth: true
        spacing: hero.size * 0.22
        Text {
            text: hero.title
            color: K.text
            font.family: Theme.displayFamily
            font.pixelSize: hero.size
            font.weight: Font.Bold
            font.letterSpacing: -hero.size * 0.01
            lineHeight: 0.95
            Accessible.role: Accessible.Heading
            Accessible.name: hero.title + " " + hero.accent
        }
        Text {
            visible: hero.accent.length > 0
            text: hero.accent
            color: Theme.c.accent
            font.family: Theme.displayFamily
            font.pixelSize: hero.size
            font.weight: Font.Bold
            font.letterSpacing: -hero.size * 0.01
            lineHeight: 0.95
        }
    }

    Text {
        Layout.fillWidth: true
        Layout.maximumWidth: 620
        visible: hero.lead.length > 0
        text: hero.lead
        color: K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 16
        lineHeight: 1.45
        wrapMode: Text.WordWrap
    }

    RowLayout {
        id: actionRow
        spacing: 12
        visible: children.length > 0
        Layout.topMargin: 4
    }
}
