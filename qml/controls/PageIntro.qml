import QtQuick
import QtQuick.Layouts
import Katsodi.App

// The website's page intro: surface panel, small uppercase eyebrow with the
// Signal line, a tight large heading and a muted lead.
Rectangle {
    id: intro
    property string eyebrow
    property string title
    property string lead
    default property alias actions: actionRow.data
    implicitHeight: content.implicitHeight + 72
    radius: 16
    color: Theme.c.surface

    ColumnLayout {
        id: content
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 36
        anchors.leftMargin: 40
        anchors.rightMargin: 40
        spacing: 14

        RowLayout {
            spacing: 10
            Rectangle { width: 22; height: 2; radius: 1; color: Theme.c.accent }
            Text {
                text: intro.eyebrow.toUpperCase()
                color: Theme.c.mutedForeground
                font.pixelSize: 9
                font.weight: Font.Bold
                font.letterSpacing: 1.44
            }
        }
        Text {
            Layout.fillWidth: true
            text: intro.title
            color: Theme.c.foreground
            font.pixelSize: Math.max(30, Math.min(44, intro.width * 0.045))
            font.weight: Font.DemiBold
            font.letterSpacing: -font.pixelSize * 0.065
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.Heading
        }
        Text {
            Layout.fillWidth: true
            Layout.maximumWidth: 560
            text: intro.lead
            visible: text.length > 0
            color: Theme.c.mutedForeground
            font.pixelSize: 15
            lineHeight: 1.5
            wrapMode: Text.WordWrap
        }
        RowLayout {
            id: actionRow
            spacing: 10
            visible: children.length > 0
        }
    }
}
