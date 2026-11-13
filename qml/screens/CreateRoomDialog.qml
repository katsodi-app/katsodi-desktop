import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// New room: a shared room on the server (signed-in accounts, same rules as
// the website) or a draft kept on this device only.
Popup {
    id: dialog
    signal createdShared(string roomId)
    signal createdDraft()
    modal: true
    focus: true
    padding: 0
    x: (parent.width - width) / 2
    y: Math.max(24, (parent.height - height) / 2)
    width: Math.min(560, parent.width - 32)
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string kind: Account.signedIn ? "shared" : "draft"
    property string format: "video"
    property string visibility: "public"
    property string errorKey: ""
    property bool working: false

    onOpened: {
        kind = Account.signedIn ? "shared" : "draft"
        errorKey = ""
        working = false
        name.text = ""
        description.text = ""
        url.text = ""
        name.forceActiveFocus()
    }

    function submit() {
        errorKey = ""
        const input = { name: name.text, description: description.text, url: url.text,
                        format: format, visibility: visibility,
                        art: ["orbit", "wave", "moon", "sun"][Math.floor(Math.random() * 4)] }
        if (kind === "draft") {
            const error = Library.createDraft(input)
            if (error) { errorKey = error; return }
            dialog.close()
            dialog.createdDraft()
            return
        }
        if (!Account.signedIn) { errorKey = "createSignIn"; return }
        // Same checks as the server, for an immediate answer.
        const local = name.text.trim().length
        if (local < 3 || local > 80) { errorKey = "createNameInvalid"; return }
        working = true
        Rooms.createShared(input)
    }

    Connections {
        target: Rooms
        function onCreated(roomId, importFailed) {
            if (!dialog.working)
                return
            dialog.working = false
            dialog.close()
            dialog.createdShared(roomId)
        }
        function onCreateFailed(messageKey) {
            if (!dialog.working)
                return
            dialog.working = false
            dialog.errorKey = messageKey
        }
    }

    Overlay.modal: Rectangle { color: Qt.alpha(Theme.c.background, 0.8) }
    enter: Transition {
        ParallelAnimation {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: K.base }
            NumberAnimation { property: "scale"; from: K.still ? 1 : 0.96; to: 1; duration: K.base; easing.type: K.easing }
        }
    }
    exit: Transition { NumberAnimation { property: "opacity"; to: 0; duration: K.fast } }
    background: Glass { solid: true; radius: K.rPanel }

    contentItem: ColumnLayout {
        spacing: 14
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.bottomMargin: 0
            Text {
                Layout.fillWidth: true
                text: I18n.s.createTitle
                color: K.text
                font.family: Theme.displayFamily
                font.pixelSize: 34
                font.weight: Font.Bold
                Accessible.role: Accessible.Heading
            }
            IconButton { iconName: "close"; tip: I18n.s.close; onClicked: dialog.close() }
        }
        Segmented {
            Layout.leftMargin: 24
            model: [{ value: "shared", label: I18n.s.createShared }, { value: "draft", label: I18n.s.createDraft }]
            current: dialog.kind
            onSelected: (value) => { dialog.kind = value; dialog.errorKey = "" }
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            text: dialog.kind === "shared" ? (Account.signedIn ? I18n.s.createSharedHint : I18n.s.createSignIn) : I18n.s.createDraftHint
            color: dialog.kind === "shared" && !Account.signedIn ? Theme.c.warning : K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
        KTextField {
            id: name
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            placeholderText: I18n.s.createName
            maximumLength: 80
        }
        KTextField {
            id: description
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            placeholderText: I18n.s.createDescription
            maximumLength: 500
        }
        KTextField {
            id: url
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            iconName: "link"
            placeholderText: I18n.s.createUrl
            maximumLength: 2048
        }
        Flow {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            spacing: 10
            Segmented {
                model: [{ value: "video", label: I18n.s.roomVideo }, { value: "music", label: I18n.s.roomMusic }]
                current: dialog.format
                onSelected: (value) => dialog.format = value
                Accessible.name: I18n.s.createFormat
            }
            Segmented {
                model: [{ value: "public", label: I18n.s.roomPublic }, { value: "private", label: I18n.s.roomPrivate }]
                current: dialog.visibility
                onSelected: (value) => dialog.visibility = value
                Accessible.name: I18n.s.createVisibility
            }
        }
        Text {
            Layout.fillWidth: true
            Layout.leftMargin: 24
            Layout.rightMargin: 24
            visible: dialog.errorKey !== ""
            text: I18n.s[dialog.errorKey] || I18n.s.errorServer
            color: Theme.c.danger
            font.family: Theme.fontFamily
            font.pixelSize: 13
            wrapMode: Text.WordWrap
            Accessible.role: Accessible.AlertMessage
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 24
            Layout.topMargin: 6
            spacing: 10
            Item { Layout.fillWidth: true }
            KButton { kind: "ghost"; text: I18n.s.cancel; onClicked: dialog.close() }
            KButton {
                kind: "accent"
                text: dialog.working ? I18n.s.createWorking : I18n.s.createSubmit
                enabled: !dialog.working && name.text.trim().length > 0 && (dialog.kind === "draft" || Account.signedIn)
                onClicked: dialog.submit()
            }
        }
    }
}
