pragma Singleton
import QtQuick
import Katsodi.App

// Design tokens of the native client, combining the website's overview page
// (night sky, Oswald headline with one accent-coloured word, frosted glass, pills)
// with Liner's calm player layout (icon rail, ambient glow, floating bar).
// Colours always come from the active Katsodi palette (Theme.c).
QtObject {
    readonly property bool still: Prefs.reduceMotion

    // Motion: short and soft; zero when motion is reduced or resources low.
    readonly property int fast: still ? 0 : 140
    readonly property int base: still ? 0 : 240
    readonly property int slow: still ? 0 : 460
    readonly property int easing: Easing.OutCubic
    readonly property int spring: Easing.OutBack

    // Shape
    readonly property int rSmall: 10
    readonly property int rCard: 18
    readonly property int rPanel: 22
    readonly property int rail: 84

    // Frosted glass over the backdrop.
    readonly property color glass: Theme.dark ? Qt.rgba(1, 1, 1, 0.055) : Qt.rgba(1, 1, 1, 0.66)
    readonly property color glassStrong: Theme.dark ? Qt.rgba(0.06, 0.07, 0.09, 0.72) : Qt.rgba(1, 1, 1, 0.86)
    readonly property color glassHover: Theme.dark ? Qt.rgba(1, 1, 1, 0.09) : Qt.rgba(1, 1, 1, 0.82)
    readonly property color stroke: Theme.dark ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(0, 0, 0, 0.08)
    readonly property color strokeStrong: Theme.dark ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(0, 0, 0, 0.16)
    readonly property color highlight: Theme.dark ? Qt.rgba(1, 1, 1, 0.16) : Qt.rgba(1, 1, 1, 0.9)

    // Text over the backdrop photo.
    readonly property color text: Theme.c.foreground
    readonly property color muted: Theme.c.mutedForeground
}
