import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Settings: palette previews, language, performance, account, about.
Scroller {
    id: page
    signal navigate(string name, string id)
    readonly property int side: width < 760 ? 20 : 48
    contentHeight: column.implicitHeight + 120

    ColumnLayout {
        id: column
        width: page.width
        spacing: 22

        PageHero {
            Layout.fillWidth: true
            Layout.leftMargin: page.side
            Layout.rightMargin: page.side
            Layout.topMargin: 48
            Layout.bottomMargin: 8
            title: I18n.s.settingsDisplay
            accent: I18n.s.settingsAccent
            lead: I18n.s.settingsIntro
            size: Math.max(44, Math.min(88, page.width * 0.07))
        }

        Card {
            title: I18n.s.appearance
            hint: I18n.s.themeHint
            Flow {
                Layout.fillWidth: true
                spacing: 14
                Repeater {
                    model: ["obsidian", "ember", "aurora", "violet", "paper", "system"]
                    delegate: AbstractButton {
                        id: option
                        required property string modelData
                        required property int index
                        readonly property var colors: Theme.paletteColors(modelData === "system" ? "obsidian" : modelData)
                        readonly property bool on: Prefs.theme === modelData
                        width: 156
                        height: 132
                        hoverEnabled: true
                        focusPolicy: Qt.StrongFocus
                        Accessible.name: modelData === "system" ? I18n.s.themeSystem : modelData
                        Accessible.checked: on
                        onClicked: Prefs.theme = modelData
                        scale: down ? 0.97 : (hovered ? 1.02 : 1)
                        Behavior on scale { NumberAnimation { duration: K.fast } }
                        background: Rectangle {
                            radius: K.rCard
                            color: option.hovered ? K.glassHover : K.glass
                            border.width: option.on || option.visualFocus ? 2 : 1
                            border.color: option.visualFocus ? Theme.c.focusRing : option.on ? Theme.c.accent : K.stroke
                            Behavior on border.color { ColorAnimation { duration: K.fast } }
                        }
                        contentItem: ColumnLayout {
                            spacing: 8
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 8
                                Layout.bottomMargin: 0
                                Layout.preferredHeight: 72
                                radius: 12
                                color: option.colors.background
                                border.width: 1
                                border.color: option.colors.border
                                Rectangle { x: 6; y: 6; width: 18; height: parent.height - 12; radius: 9; color: option.colors.surface }
                                Rectangle { x: 32; y: 12; width: 64; height: 8; radius: 4; color: option.colors.foreground; opacity: 0.9 }
                                Rectangle { x: 32; y: 26; width: 40; height: 6; radius: 3; color: option.colors.accent }
                                Row {
                                    x: 32; y: 42; spacing: 5
                                    Repeater { model: 3; Rectangle { width: 24; height: 20; radius: 6; color: option.colors.surfaceElevated } }
                                }
                            }
                            Text {
                                Layout.fillWidth: true
                                Layout.leftMargin: 12
                                text: option.modelData === "system" ? I18n.s.themeSystem
                                      : option.modelData.charAt(0).toUpperCase() + option.modelData.slice(1)
                                color: K.text
                                font.family: Theme.fontFamily
                                font.pixelSize: 13
                                font.weight: Font.Bold
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
            Segmented {
                model: [{ value: "ru", label: "Русский" }, { value: "en", label: "English" }]
                current: Prefs.locale
                onSelected: (value) => Prefs.locale = value
            }
        }

        Card {
            title: I18n.s.performance
            hint: I18n.s.performanceHint
            Segmented {
                model: [{ value: "auto", label: I18n.s.resourceAuto },
                        { value: "standard", label: I18n.s.resourceStandard },
                        { value: "low", label: I18n.s.resourceLow }]
                current: Prefs.resourceMode
                onSelected: (value) => Prefs.resourceMode = value
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
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
            RowLayout {
                KButton { visible: Account.signedIn; kind: "ghost"; iconName: "leave"; text: I18n.s.signOut; onClicked: Account.signOut() }
                KButton { visible: !Account.signedIn; text: I18n.s.signIn; onClicked: page.navigate("signin", "") }
            }
        }

        Text {
            Layout.leftMargin: page.side
            Layout.bottomMargin: 32
            text: I18n.format("aboutApp", "version", appVersion) + (appChannel === "development" ? " · dev" : "")
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 11
        }
    }

    component Card: Glass {
        id: card
        property string title
        property string hint
        default property alias body: inner.data
        Layout.fillWidth: true
        Layout.leftMargin: page.side
        Layout.rightMargin: page.side
        implicitHeight: cardColumn.implicitHeight + 52
        radius: K.rPanel
        ColumnLayout {
            id: cardColumn
            anchors.fill: parent
            anchors.margins: 26
            spacing: 14
            Text {
                text: card.title
                color: K.text
                font.family: Theme.fontFamily
                font.pixelSize: 19
                font.weight: Font.Bold
                font.letterSpacing: -0.4
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: card.hint
                color: K.muted
                font.family: Theme.fontFamily
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
