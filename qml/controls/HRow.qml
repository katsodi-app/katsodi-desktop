import QtQuick
import Katsodi.App
import Katsodi.Ui

// Horizontal shelf of cards (Liner rows). Vertical wheel turns go to the
// page scroller; sideways turns and drags move the shelf.
ListView {
    id: shelf
    property Flickable page: null
    orientation: ListView.Horizontal
    spacing: 18
    clip: false
    boundsBehavior: Flickable.StopAtBounds
    flickDeceleration: 3200
    cacheBuffer: 600
    reuseItems: true

    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        onWheel: (event) => {
            if (Math.abs(event.angleDelta.x) > Math.abs(event.angleDelta.y)) {
                const max = Math.max(0, shelf.contentWidth - shelf.width)
                shelf.contentX = Math.max(0, Math.min(max, shelf.contentX - event.angleDelta.x))
            } else if (shelf.page) {
                shelf.page.wheel(event)
            }
        }
    }
}
