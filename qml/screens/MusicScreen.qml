import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// The music library, as on the website: shelves (all-time top, the
// providers' weekly charts, popular, trending), a filter, and "Listen in a
// room" for every track.
Item {
    id: page
    signal navigate(string name, string id)
    readonly property int side: width < 760 ? 20 : 40
    readonly property var providers: ({ spotify: "Spotify", youtube: "YouTube", soundcloud: "SoundCloud",
                                        apple: "Apple Music", tiktok: "TikTok" })

    function shelfLabel(s) {
        if (s.id === "all-time") return I18n.s.musicShelfAllTime
        if (s.id === "popular") return I18n.s.musicShelfPopular
        if (s.id === "trending") return I18n.s.musicShelfTrending
        return I18n.s.musicShelfChart.replace("{provider}", providers[s.provider] || s.provider)
    }
    function compact(n) {
        if (n >= 1e9) return (n / 1e9).toFixed(1) + "B"
        if (n >= 1e6) return (n / 1e6).toFixed(1) + "M"
        if (n >= 1e3) return Math.round(n / 1e3) + "K"
        return String(Math.round(n))
    }
    function clock(s) { return Math.floor(s / 60) + ":" + String(s % 60).padStart(2, "0") }

    Component.onCompleted: Music.reload()
    Connections {
        target: Music
        function onOpenRoom(roomId) { page.navigate("room", roomId) }
    }

    Scroller {
        id: scroller
        anchors.fill: parent
        contentHeight: column.implicitHeight + 80

        ColumnLayout {
            id: column
            x: page.side
            width: scroller.width - page.side * 2
            spacing: 22

            PageHero {
                Layout.fillWidth: true
                Layout.topMargin: 48
                badge: I18n.s.musicEyebrow
                title: I18n.s.musicDisplay
                accent: I18n.s.musicAccent
                lead: I18n.s.musicIntro
                size: Math.max(44, Math.min(88, page.width * 0.07))
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 12
                KTextField {
                    Layout.preferredWidth: 380
                    Layout.maximumWidth: 380
                    iconName: "search"
                    placeholderText: I18n.s.musicSearch
                    text: Music.query
                    onTextEdited: Music.query = text
                }
                Item { Layout.fillWidth: true }
            }

            // Shelves, scrolling sideways when there are many.
            Flickable {
                Layout.fillWidth: true
                implicitHeight: shelfRow.implicitHeight
                contentWidth: shelfRow.implicitWidth
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Segmented {
                    id: shelfRow
                    model: Music.shelves.map(s => ({ value: s.id, label: page.shelfLabel(s) }))
                    current: Music.shelf
                    onSelected: (value) => Music.shelf = value
                }
            }
            Text {
                readonly property var current: Music.shelves.find(s => s.id === Music.shelf)
                visible: !!current && !!current.checked
                text: current ? I18n.s.musicChecked.replace("{date}", current.checked) + (current.stale ? "  ·  " + I18n.s.musicStale : "") : ""
                color: current && current.stale ? Theme.c.warning : K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 12
            }

            Text {
                visible: Music.errorKey !== ""
                text: I18n.s[Music.errorKey] || I18n.s.errorServer
                color: Theme.c.danger
                font.family: Theme.fontFamily
            }
            BusyIndicator { visible: Music.loading && Music.tracks.length === 0; running: visible && !K.still }
            Text {
                visible: !Music.loading && Music.tracks.length === 0 && Music.errorKey === ""
                text: I18n.s.musicEmpty
                color: K.muted
                font.family: Theme.fontFamily
            }

            Repeater {
                model: Music.tracks
                delegate: Glass {
                    id: row
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: 70
                    radius: 16
                    hoverable: true
                    HoverHandler { id: hover }
                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 12
                        spacing: 14
                        Text {
                            Layout.preferredWidth: 34
                            horizontalAlignment: Text.AlignRight
                            text: row.modelData.rank || (row.index + 1)
                            color: row.index < 3 ? Theme.c.accent : K.muted
                            font.family: Theme.displayFamily
                            font.pixelSize: 20
                            font.weight: Font.Bold
                        }
                        Rectangle {
                            implicitWidth: 50; implicitHeight: 50; radius: 10
                            color: Theme.c.selected
                            clip: true
                            Icon { anchors.centerIn: parent; name: "music"; size: 20; color: Theme.c.accent; visible: art.status !== Image.Ready }
                            Image {
                                id: art
                                anchors.fill: parent
                                source: row.modelData.cover
                                sourceSize: Qt.size(100, 100)
                                fillMode: Image.PreserveAspectCrop
                                asynchronous: true
                                cache: true
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.title
                                color: K.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 15
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                            Text {
                                Layout.fillWidth: true
                                text: row.modelData.artists
                                      + (row.modelData.plays > 0 ? "  ·  " + I18n.s.musicPlays.replace("{n}", page.compact(row.modelData.plays)) : "")
                                      + (row.modelData.duration > 0 ? "  ·  " + page.clock(row.modelData.duration) : "")
                                color: K.muted
                                font.family: Theme.fontFamily
                                font.pixelSize: 12
                                elide: Text.ElideRight
                            }
                        }
                        KButton {
                            small: true
                            kind: hover.hovered ? "primary" : "ghost"
                            iconName: "play"
                            text: Music.roomTakesTracks ? I18n.s.musicListenHere : I18n.s.musicListen
                            enabled: Account.signedIn && !Music.busy
                            ToolTip.visible: hovered && !Account.signedIn
                            ToolTip.text: I18n.s.musicSignIn
                            onClicked: Music.listen(row.modelData.url, row.modelData.title)
                        }
                    }
                }
            }
        }
    }
}
