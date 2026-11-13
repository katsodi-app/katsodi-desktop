import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// What to share, in the manner of Discord's picker: "Applications" and
// "Entire screen" tabs, a grid of live previews with each app's icon and
// title, then quality and smoothness, and Share. Previews are taken on this
// computer for this picker only and are never sent.
Popup {
    id: picker
    parent: Overlay.overlay
    modal: true
    focus: true
    padding: 0
    x: Math.round((parent.width - width) / 2)
    y: Math.round(Math.max(24, (parent.height - height) / 2))
    width: Math.min(980, parent.width - 48)
    height: Math.min(760, parent.height - 48)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string tab: "apps"
    property int chosen: -1
    readonly property var visibleSources: {
        const wanted = tab === "apps" ? "window" : "screen"
        return Demo.sources.filter(s => s.kind === wanted)
    }

    onOpened: {
        chosen = -1
        tab = "apps"
        Demo.refreshSources()
    }

    Overlay.modal: Rectangle {
        color: Qt.alpha(Theme.c.background, 0.82)
        Behavior on opacity { NumberAnimation { duration: K.base } }
    }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base }
            NumberAnimation { property: "scale"; from: K.still ? 1 : 0.97; to: 1; duration: K.base; easing.type: K.easing }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }
    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 0

        // Tabs, full width like Discord's.
        Rectangle {
            Layout.fillWidth: true
            Layout.margins: 16
            Layout.bottomMargin: 8
            implicitHeight: 48
            radius: 14
            color: K.glass
            border.width: 1
            border.color: K.stroke
            RowLayout {
                anchors.fill: parent
                anchors.margins: 4
                spacing: 4
                Repeater {
                    model: [{ value: "apps", label: I18n.s.demoTabApps, icon: "app" },
                            { value: "screens", label: I18n.s.demoTabScreens, icon: "monitor" }]
                    delegate: AbstractButton {
                        id: tabButton
                        required property var modelData
                        readonly property bool on: picker.tab === modelData.value
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        hoverEnabled: true
                        onClicked: { picker.tab = modelData.value; picker.chosen = -1 }
                        background: Rectangle {
                            radius: 10
                            color: tabButton.on ? K.glassHover : (tabButton.hovered ? K.glass : "transparent")
                            Behavior on color { ColorAnimation { duration: K.fast } }
                        }
                        contentItem: Item {
                            Row {
                                anchors.centerIn: parent
                                spacing: 8
                                Icon { anchors.verticalCenter: parent.verticalCenter; name: tabButton.modelData.icon; size: 17
                                       color: tabButton.on ? K.text : K.muted }
                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: tabButton.modelData.label
                                    color: tabButton.on ? K.text : K.muted
                                    font.family: Theme.fontFamily
                                    font.pixelSize: 14
                                    font.weight: Font.Bold
                                }
                            }
                        }
                    }
                }
            }
        }

        // Previews.
        GridView {
            id: grid
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: 16
            Layout.rightMargin: 8
            clip: true
            model: picker.visibleSources
            readonly property int columns: width > 760 ? 3 : 2
            cellWidth: Math.floor(width / columns)
            cellHeight: Math.round(cellWidth * 9 / 16) + 44
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}
            delegate: AbstractButton {
                id: tile
                required property var modelData
                readonly property bool on: picker.chosen === modelData.index
                width: grid.cellWidth - 8
                height: grid.cellHeight - 8
                hoverEnabled: true
                onClicked: picker.chosen = modelData.index
                onDoubleClicked: picker.go(modelData.index)
                Accessible.name: modelData.name
                background: Item {}
                contentItem: ColumnLayout {
                    spacing: 8
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 12
                        color: "#0b0b0e"
                        border.width: tile.on ? 3 : 1
                        border.color: tile.on ? Theme.c.primary : (tile.hovered ? K.strokeStrong : K.stroke)
                        Behavior on border.color { ColorAnimation { duration: K.fast } }
                        clip: true
                        Image {
                            anchors.fill: parent
                            anchors.margins: tile.on ? 3 : 1
                            source: tile.modelData.preview || ""
                            fillMode: Image.PreserveAspectFit
                            asynchronous: true
                            smooth: true
                            visible: source !== ""
                        }
                        Icon {
                            anchors.centerIn: parent
                            visible: !tile.modelData.preview
                            name: tile.modelData.kind === "screen" ? "monitor" : "app"
                            size: 34
                            color: K.muted
                        }
                        scale: tile.hovered && !K.still ? 1.015 : 1
                        Behavior on scale { NumberAnimation { duration: K.fast; easing.type: K.easing } }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Image {
                            Layout.preferredWidth: 20
                            Layout.preferredHeight: 20
                            source: tile.modelData.icon || ""
                            visible: source !== ""
                            smooth: true
                        }
                        Icon {
                            visible: !tile.modelData.icon
                            name: tile.modelData.kind === "screen" ? "monitor" : "app"
                            size: 18
                            color: K.muted
                        }
                        Text {
                            Layout.fillWidth: true
                            text: tile.modelData.name
                            color: K.text
                            font.family: Theme.fontFamily
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            elide: Text.ElideRight
                        }
                    }
                }
            }
            BusyIndicator {
                anchors.centerIn: parent
                visible: Demo.loadingSources && picker.tab === "apps" && grid.count === 0
                running: visible && !K.still
            }
            Text {
                anchors.centerIn: parent
                visible: !Demo.loadingSources && grid.count === 0
                text: I18n.s.demoNoSources
                color: K.muted
                font.family: Theme.fontFamily
            }
        }

        // Quality, smoothness and the action.
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: footer.implicitHeight + 32
            color: K.glass
            border.width: 0
            radius: K.rPanel
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; height: K.rPanel; color: K.glass }
            RowLayout {
                id: footer
                anchors.fill: parent
                anchors.margins: 16
                spacing: 18
                ColumnLayout {
                    spacing: 6
                    Text { text: I18n.s.demoQuality; color: K.muted; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold }
                    Segmented {
                        model: [{ value: "720", label: "720p" }, { value: "1080", label: "1080p" }]
                        current: String(Demo.quality)
                        onSelected: (value) => Demo.quality = Number(value)
                    }
                }
                ColumnLayout {
                    spacing: 6
                    Text { text: I18n.s.demoSmooth; color: K.muted; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold }
                    Segmented {
                        model: [{ value: "15", label: "15 fps" }, { value: "30", label: "30 fps" }]
                        current: String(Demo.framesPerSecond)
                        onSelected: (value) => Demo.framesPerSecond = Number(value)
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 6
                    Text { text: I18n.s.demoAudio; color: K.muted; font.family: Theme.fontFamily; font.pixelSize: 12; font.weight: Font.Bold }
                    KSwitch {
                        checked: Demo.shareAudio
                        onToggled: Demo.shareAudio = checked
                        Accessible.name: I18n.s.demoAudio
                    }
                }
                KButton {
                    kind: "ghost"
                    text: I18n.s.cancel
                    onClicked: picker.close()
                }
                KButton {
                    kind: "primary"
                    iconName: "monitor"
                    text: I18n.s.demoGo
                    enabled: picker.chosen >= 0
                    onClicked: picker.go(picker.chosen)
                }
            }
        }
    }

    function go(index) {
        if (index < 0)
            return
        Demo.share(index)
        picker.close()
    }
}
