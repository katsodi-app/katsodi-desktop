import QtQuick
import QtQuick.Controls
import Katsodi.App
import Katsodi.Ui

// Vertical page scroller with eased mouse-wheel scrolling (precise
// touchpads scroll 1:1) and a slim overlay scroll bar. Horizontal rows
// inside forward vertical wheel turns here through wheel().
Flickable {
    id: scroller
    clip: true
    contentWidth: width
    boundsBehavior: Flickable.StopAtBounds
    flickDeceleration: 3200
    maximumFlickVelocity: 6000
    property real target: 0

    function wheel(event) {
        const pixel = event.pixelDelta.y !== 0
        const delta = pixel ? event.pixelDelta.y : event.angleDelta.y * 1.15
        const max = Math.max(0, contentHeight - height)
        target = Math.max(0, Math.min(max, (glide.running ? target : contentY) - delta))
        if (K.still || pixel) {
            glide.stop()
            contentY = target
            return
        }
        glide.to = target
        glide.restart()
    }
    function scrollToTop() {
        target = 0
        glide.to = 0
        glide.restart()
    }

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => scroller.wheel(event)
    }
    NumberAnimation {
        id: glide
        target: scroller
        property: "contentY"
        duration: 300
        easing.type: Easing.OutCubic
    }

    ScrollBar.vertical: ScrollBar {
        id: bar
        policy: ScrollBar.AsNeeded
        width: 10
        contentItem: Rectangle {
            implicitWidth: 4
            radius: 2
            color: Theme.c.foreground
            opacity: bar.active ? 0.35 : 0
            Behavior on opacity { NumberAnimation { duration: K.base } }
        }
        background: Item {}
    }
}
