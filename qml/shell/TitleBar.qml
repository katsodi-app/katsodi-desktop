import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Our own title bar for the frameless window: drag to move (Windows snap
// still works, the move is the system's), double-click to maximize or
// restore, and minimize / maximize / close on the right.
Item {
    id: bar
    required property var window
    implicitHeight: 36
    readonly property bool maximized: window.visibility === Window.Maximized

    function toggleMaximized() {
        if (bar.maximized)
            window.showNormal()
        else
            window.showMaximized()
    }

    // The empty part of the bar moves the window.
    Item {
        anchors.fill: parent
        DragHandler {
            target: null
            grabPermissions: PointerHandler.TakeOverForbidden
            onActiveChanged: if (active) bar.window.startSystemMove()
        }
        TapHandler {
            acceptedButtons: Qt.LeftButton
            onDoubleTapped: bar.toggleMaximized()
        }
    }

    Row {
        anchors.right: parent.right
        anchors.top: parent.top
        height: parent.height
        WindowButton {
            iconName: "minimize"
            tip: I18n.s.windowMinimize
            onClicked: bar.window.showMinimized()
        }
        WindowButton {
            iconName: bar.maximized ? "restore" : "maximize"
            tip: bar.maximized ? I18n.s.windowRestore : I18n.s.windowMaximize
            onClicked: bar.toggleMaximized()
        }
        WindowButton {
            iconName: "close"
            tip: I18n.s.windowClose
            danger: true
            onClicked: bar.window.close()
        }
    }

    component WindowButton: AbstractButton {
        id: button
        property string iconName
        property string tip
        property bool danger: false
        width: 46
        height: bar.height
        hoverEnabled: true
        focusPolicy: Qt.NoFocus
        Accessible.name: tip
        ToolTip.visible: hovered
        ToolTip.delay: 600
        ToolTip.text: tip
        background: Rectangle {
            color: button.danger && button.hovered ? "#c42b1c"
                   : button.down ? K.glassHover
                   : button.hovered ? K.glass : "transparent"
            Behavior on color { ColorAnimation { duration: K.fast } }
        }
        contentItem: Item {
            Icon {
                anchors.centerIn: parent
                name: button.iconName
                size: 15
                stroke: 1.6
                color: button.danger && button.hovered ? "#ffffff" : (button.hovered ? K.text : K.muted)
            }
        }
    }
}
