import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Shapes
import Katsodi.App

// A sidebar entry: 48 px, radius 12, elevated surface and the accent
// "signal" bar when it is the current page (website .sidebar a[aria-current]).
ItemDelegate {
    id: item
    property string label
    property string glyph
    property bool active: false
    Layout.fillWidth: true
    Layout.bottomMargin: 7
    implicitHeight: 48
    hoverEnabled: true
    focusPolicy: Qt.StrongFocus
    Accessible.name: label
    Accessible.role: Accessible.Link

    background: Rectangle {
        radius: 12
        color: item.active || item.hovered ? Theme.c.surfaceElevated : "transparent"
        Rectangle {
            visible: item.active
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 3; height: 18; radius: 2
            color: Theme.c.accent
        }
        Rectangle {
            anchors.fill: parent
            anchors.margins: -2
            radius: 14
            color: "transparent"
            border.width: 2
            border.color: Theme.c.focusRing
            visible: item.visualFocus
        }
    }
    contentItem: RowLayout {
        spacing: 12
        // Simple line glyphs drawn natively (stand-ins for the website's
        // Lucide icons: house, users, sparkles, sliders).
        Shape {
            implicitWidth: 17
            implicitHeight: 17
            preferredRendererType: Shape.CurveRenderer
            ShapePath {
                strokeColor: item.active ? Theme.c.accent : Theme.c.mutedForeground
                strokeWidth: 1.6
                fillColor: "transparent"
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathSvg {
                    path: item.glyph === "home" ? "M2 8 L8.5 2.5 L15 8 V15 H11 V10.5 H6 V15 H2 Z"
                          : item.glyph === "rooms" ? "M6 8 A3 3 0 1 0 6 2 A3 3 0 1 0 6 8 M1 15 C1 11.5 3 10 6 10 C9 10 11 11.5 11 15 M12 2.5 A2.6 2.6 0 0 1 12 7.8 M13 10.3 C15 10.8 16 12.5 16 15"
                          : item.glyph === "fandoms" ? "M8.5 1.5 L10 6.5 L15 8 L10 9.5 L8.5 14.5 L7 9.5 L2 8 L7 6.5 Z"
                          : "M2 4 H10 M13 4 H15 M2 8.5 H5 M8 8.5 H15 M2 13 H9 M12 13 H15 M11.5 2.5 V5.5 M6.5 7 V10 M10.5 11.5 V14.5"
                }
            }
        }
        Text {
            Layout.fillWidth: true
            text: item.label
            color: item.active ? Theme.c.foreground : Theme.c.mutedForeground
            font.pixelSize: 13
            font.weight: Font.Medium
            elide: Text.ElideRight
        }
    }
}
