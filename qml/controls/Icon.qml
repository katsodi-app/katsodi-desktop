import QtQuick
import QtQuick.Shapes
import Katsodi.App
import Katsodi.Ui

// Line icons drawn natively on a 24-unit grid (Lucide-like strokes).
Item {
    id: icon
    property string name: "home"
    property color color: Theme.c.foreground
    property real stroke: 1.8
    property int size: 20
    implicitWidth: size
    implicitHeight: size

    readonly property var paths: ({
        home: "M3 10.5 L12 3 L21 10.5 M5.5 9 V20 H10 V14.5 H14 V20 H18.5 V9",
        rooms: "M9 11 A3.5 3.5 0 1 0 9 4 A3.5 3.5 0 1 0 9 11 M2.5 20 C2.5 15.5 5.5 13.5 9 13.5 C12.5 13.5 15.5 15.5 15.5 20 M16 4.5 A3 3 0 0 1 16 10.5 M18 13.8 C20.3 14.6 21.5 16.6 21.5 20",
        fandoms: "M12 2.5 L14.2 9.3 L21.5 12 L14.2 14.7 L12 21.5 L9.8 14.7 L2.5 12 L9.8 9.3 Z",
        library: "M6 3.5 H18 V21 L12 16.5 L6 21 Z",
        settings: "M4 6 H13 M17 6 H20 M4 12 H7 M11 12 H20 M4 18 H14 M18 18 H20 M15 4 V8 M9 10 V14 M16 16 V20",
        search: "M10.5 17.5 A7 7 0 1 0 10.5 3.5 A7 7 0 1 0 10.5 17.5 M15.5 15.5 L21 21",
        plus: "M12 5 V19 M5 12 H19",
        close: "M6 6 L18 18 M18 6 L6 18",
        back: "M15 5 L8 12 L15 19",
        chevron: "M9 5 L16 12 L9 19",
        mic: "M12 3 A3 3 0 0 1 15 6 V12 A3 3 0 0 1 9 12 V6 A3 3 0 0 1 12 3 M5.5 11 A6.5 6.5 0 0 0 18.5 11 M12 17.5 V21",
        micOff: "M3 3 L21 21 M15 9.5 V6 A3 3 0 0 0 9.2 4.9 M9 9 V12 A3 3 0 0 0 14.1 14.1 M18.5 11 A6.5 6.5 0 0 1 17.5 14.5 M5.5 11 A6.5 6.5 0 0 0 15.2 16.7 M12 17.5 V21",
        headphones: "M3.5 17 V12.5 A8.5 8.5 0 0 1 20.5 12.5 V17 M3.5 14 H7 V20 H5 A1.5 1.5 0 0 1 3.5 18.5 Z M20.5 14 H17 V20 H19 A1.5 1.5 0 0 0 20.5 18.5 Z",
        headphonesOff: "M3 3 L21 21 M3.5 17 V12.5 A8.5 8.5 0 0 1 6 6.5 M9.3 4.5 A8.5 8.5 0 0 1 20.5 12.5 V16 M3.5 14 H7 V20 H5 A1.5 1.5 0 0 1 3.5 18.5 Z",
        leave: "M10 4 H5 V20 H10 M15 8 L19 12 L15 16 M19 12 H9",
        bookmark: "M6 3.5 H18 V21 L12 16.5 L6 21 Z",
        star: "M12 3 L14.6 9 L21 9.6 L16.1 13.8 L17.6 20.2 L12 16.9 L6.4 20.2 L7.9 13.8 L3 9.6 L9.4 9 Z",
        play: "M7 4.5 L19 12 L7 19.5 Z",
        pause: "M8 5 V19 M16 5 V19",
        queue: "M4 6 H20 M4 12 H20 M4 18 H13 M17 15.5 L21 18 L17 20.5 Z",
        link: "M10 14 A4.5 4.5 0 0 0 16.4 14 L19.5 10.9 A4.5 4.5 0 0 0 13.1 4.5 L11.5 6.1 M14 10 A4.5 4.5 0 0 0 7.6 10 L4.5 13.1 A4.5 4.5 0 0 0 10.9 19.5 L12.5 17.9",
        sliders: "M6 4 V20 M18 4 V20 M12 4 V20 M3.5 14 H8.5 M15.5 9 H20.5 M9.5 10 H14.5",
        trash: "M4 7 H20 M9 7 V4.5 H15 V7 M6.5 7 L7.5 20 H16.5 L17.5 7",
        globe: "M12 21 A9 9 0 1 0 12 3 A9 9 0 1 0 12 21 M3.5 12 H20.5 M12 3 C9.5 6 9.5 18 12 21 M12 3 C14.5 6 14.5 18 12 21",
        wave: "M4 10 V14 M8 7 V17 M12 4 V20 M16 8 V16 M20 10 V14",
        music: "M9 18 V5.5 L20 3.5 V16 M9 18 A3 3 0 1 1 3 18 A3 3 0 1 1 9 18 M20 16 A3 3 0 1 1 14 16 A3 3 0 1 1 20 16",
        video: "M3.5 6 H15.5 V18 H3.5 Z M15.5 10 L20.5 7 V17 L15.5 14",
        lock: "M6 11 H18 V20 H6 Z M8.5 11 V8 A3.5 3.5 0 0 1 15.5 8 V11",
        flame: "M12 21 C7.5 21 5.5 17.5 6.5 14 C7.3 11.3 9.5 10 9.5 7 C11.5 8.2 12.5 10 12.3 11.8 C13.5 10.8 14 9 13.7 6.5 C16.8 8.5 18.5 11.8 17.8 15.2 C17.2 18.6 15 21 12 21",
        clock: "M12 21 A9 9 0 1 0 12 3 A9 9 0 1 0 12 21 M12 7 V12 L15.5 14"
    })

    Shape {
        anchors.centerIn: parent
        width: 24
        height: 24
        scale: icon.size / 24
        preferredRendererType: Shape.CurveRenderer
        ShapePath {
            strokeColor: icon.color
            strokeWidth: icon.stroke * 24 / icon.size
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] || icon.paths.home }
        }
    }
}
