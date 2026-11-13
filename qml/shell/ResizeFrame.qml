import QtQuick

// Resize handles around the frameless window: edges and corners hand the
// drag to the system (startSystemResize), so sizing feels native and
// respects the minimum size. Hidden while maximized or full screen.
Item {
    id: frame
    required property var window
    property int grip: 6
    anchors.fill: parent
    visible: window.visibility === Window.Windowed
    z: 1000

    component Grip: MouseArea {
        property int edges
        acceptedButtons: Qt.LeftButton
        onPressed: frame.window.startSystemResize(edges)
    }

    Grip { edges: Qt.LeftEdge; cursorShape: Qt.SizeHorCursor
           x: 0; y: frame.grip; width: frame.grip; height: parent.height - 2 * frame.grip }
    Grip { edges: Qt.RightEdge; cursorShape: Qt.SizeHorCursor
           x: parent.width - frame.grip; y: frame.grip; width: frame.grip; height: parent.height - 2 * frame.grip }
    Grip { edges: Qt.TopEdge; cursorShape: Qt.SizeVerCursor
           x: frame.grip; y: 0; width: parent.width - 2 * frame.grip; height: frame.grip }
    Grip { edges: Qt.BottomEdge; cursorShape: Qt.SizeVerCursor
           x: frame.grip; y: parent.height - frame.grip; width: parent.width - 2 * frame.grip; height: frame.grip }
    Grip { edges: Qt.LeftEdge | Qt.TopEdge; cursorShape: Qt.SizeFDiagCursor
           x: 0; y: 0; width: frame.grip * 2; height: frame.grip * 2 }
    Grip { edges: Qt.RightEdge | Qt.BottomEdge; cursorShape: Qt.SizeFDiagCursor
           x: parent.width - frame.grip * 2; y: parent.height - frame.grip * 2; width: frame.grip * 2; height: frame.grip * 2 }
    Grip { edges: Qt.RightEdge | Qt.TopEdge; cursorShape: Qt.SizeBDiagCursor
           x: parent.width - frame.grip * 2; y: 0; width: frame.grip * 2; height: frame.grip * 2 }
    Grip { edges: Qt.LeftEdge | Qt.BottomEdge; cursorShape: Qt.SizeBDiagCursor
           x: 0; y: parent.height - frame.grip * 2; width: frame.grip * 2; height: frame.grip * 2 }
}
