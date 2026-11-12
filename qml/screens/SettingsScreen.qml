import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Settings: appearance (palette previews), language, performance, account.
ScrollView {
    id: screen
    signal navigate(string name, string id)
    contentWidth: availableWidth
    clip: true
    readonly property int side: width < 860 ? 16 : 44

    ColumnLayout {
        width: screen.availableWidth
        spacing: 20

        PageIntro {
            Layout.fillWidth: true
            Layout.leftMargin: screen.side
            Layout.rightMargin: screen.side
            Layout.topMargin: 4
            eyebrow: I18n.s.settingsEyebrow
            title: I18n.s.settingsTitle
            lead: I18n.s.settingsIntro
        }

        Card {
            title: I18n.s.appearance
            hint: I18n.s.themeHint
            Flow {
                Layout.fillWidth: true
                spacing: 12
                Repeater {
                    model: ["obsidian", "ember", "aurora", "violet", "paper", "system"]
                    delegate: ItemDelegate {
                        id: option
                        required property string modelData
                        readonly property var colors: Theme.paletteColors(modelData === "system" ? "obsidian" : modelData)
                        width: 150
                        height: 128
                        padding: 8
                        hoverEnabled: true
                        Accessible.name: modelData === "system" ? I18n.s.themeSystem : modelData
                        Accessible.checked: Prefs.theme === modelData
                        onClicked: Prefs.theme = modelData
                        background: Rectangle {
                            radius: 14
                            color: Theme.c.surfaceElevated
                            border.width: Prefs.theme === option.modelData || option.visualFocus ? 2 : 1
                            border.color: option.visualFocus ? Theme.c.focusRing
                                          : Prefs.theme === option.modelData ? Theme.c.accent : Theme.c.border
                        }
                        contentItem: ColumnLayout {
                            spacing: 8
                            // Miniature of the palette, like the website's theme preview.
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 70
                                radius: 9
                                color: option.colors.background
                                border.width: 1
                                border.color: option.colors.border
                                Rectangle { x: 6; y: 6; width: 26; height: parent.height - 12; radius: 5; color: option.colors.surface }
                                Rectangle { x: 40; y: 10; width: 60; height: 8; radius: 4; color: option.colors.foreground; opacity: 0.85 }
                                Rectangle { x: 40; y: 24; width: 36; height: 6; radius: 3; color: option.colors.accent }
                                Row {
                                    x: 40; y: 40; spacing: 5
                                    Repeater { model: 3; Rectangle { width: 22; height: 18; radius: 4; color: option.colors.surfaceElevated } }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                text: option.modelData === "system" ? I18n.s.themeSystem
                                      : option.modelData.charAt(0).toUpperCase() + option.modelData.slice(1)
                                color: Theme.c.foreground
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                            }
                        }
                    }
                }
            }
        }

        Card {
            title: I18n.s.language
            hint: I18n.s.languageHint
            RowLayout {
                spacing: 8
                KChip { text: "Русский"; selected: Prefs.locale === "ru"; onClicked: Prefs.locale = "ru" }
                KChip { text: "English"; selected: Prefs.locale === "en"; onClicked: Prefs.locale = "en" }
            }
        }

        Card {
            title: I18n.s.performance
            hint: I18n.s.performanceHint
            RowLayout {
                spacing: 8
                KChip { text: I18n.s.resourceAuto; selected: Prefs.resourceMode === "auto"; onClicked: Prefs.resourceMode = "auto" }
                KChip { text: I18n.s.resourceStandard; selected: Prefs.resourceMode === "standard"; onClicked: Prefs.resourceMode = "standard" }
                KChip { text: I18n.s.resourceLow; selected: Prefs.resourceMode === "low"; onClicked: Prefs.resourceMode = "low" }
            }
        }

        Card {
            title: I18n.s.account
            hint: Account.signedIn ? I18n.format("accountSignedIn", "name", Account.displayName) : I18n.s.accountSignedOut
            Text {
                Layout.fillWidth: true
                visible: !Account.persistent
                text: I18n.s.accountSessionOnly
                color: Theme.c.warning
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                KButton {
                    visible: Account.signedIn
                    kind: "secondary"
                    text: I18n.s.signOut
                    onClicked: Account.signOut()
                }
                KButton {
                    visible: !Account.signedIn
                    text: I18n.s.signIn
                    onClicked: screen.navigate("signin", "")
                }
            }
        }

        Text {
            Layout.leftMargin: screen.side
            Layout.bottomMargin: 32
            text: I18n.format("aboutApp", "version", appVersion) + (appChannel === "development" ? " · dev" : "")
            color: Theme.c.mutedForeground
            font.pixelSize: 11
        }
    }

    component Card: Rectangle {
        id: card
        property string title
        property string hint
        default property alias body: inner.data
        Layout.fillWidth: true
        Layout.leftMargin: screen.side
        Layout.rightMargin: screen.side
        implicitHeight: column.implicitHeight + 48
        radius: 16
        color: Theme.c.surface
        ColumnLayout {
            id: column
            anchors.fill: parent
            anchors.margins: 24
            spacing: 14
            Text {
                text: card.title
                color: Theme.c.foreground
                font.pixelSize: 18
                font.weight: Font.DemiBold
                font.letterSpacing: -0.5
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: card.hint
                color: Theme.c.mutedForeground
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
            ColumnLayout {
                id: inner
                Layout.fillWidth: true
                spacing: 10
            }
        }
    }
}
