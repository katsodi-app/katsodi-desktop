import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// The owner's invitations, as on the website: create one (the link or code
// is shown once), see the live ones and revoke them.
ColumnLayout {
    id: panel
    spacing: 10

    function copy(text) {
        clipboard.text = text
        clipboard.selectAll()
        clipboard.copy()
        clipboard.text = ""
        copied.restart()
    }
    // A hidden editor is the plain way to reach the clipboard from QML.
    TextEdit { id: clipboard; visible: false }
    Timer { id: copied; interval: 1600 }

    Text {
        text: I18n.s.inviteTitle
        color: K.text
        font.family: Theme.displayFamily
        font.pixelSize: 16
        font.weight: Font.Bold
    }
    Text {
        Layout.fillWidth: true
        text: I18n.s.inviteHint
        color: K.muted
        font.family: Theme.fontFamily
        font.pixelSize: 12
        wrapMode: Text.WordWrap
    }
    KButton {
        kind: "ghost"
        small: true
        iconName: "link"
        text: I18n.s.inviteCreate
        enabled: !Room.moderating
        onClicked: Room.issueInvite()
    }

    // The one just issued: link (or code when the server has no public origin).
    ColumnLayout {
        id: issued
        Layout.fillWidth: true
        visible: !!Room.issuedInvite.id
        spacing: 6
        readonly property string shown: Room.issuedInvite.link || Room.issuedInvite.token || ""
        Text {
            text: Room.issuedInvite.link ? I18n.s.inviteLink : I18n.s.inviteCode
            color: K.muted
            font.family: Theme.fontFamily
            font.pixelSize: 11
            font.weight: Font.DemiBold
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            KTextField {
                Layout.fillWidth: true
                readOnly: true
                text: issued.shown
                onActiveFocusChanged: if (activeFocus) selectAll()
            }
            KButton {
                small: true
                kind: "primary"
                text: copied.running ? I18n.s.inviteCopied : I18n.s.inviteCopy
                onClicked: panel.copy(issued.shown)
            }
        }
        Text {
            Layout.fillWidth: true
            visible: !Room.issuedInvite.link
            text: I18n.s.inviteNoLink
            color: Theme.c.warning
            font.family: Theme.fontFamily
            font.pixelSize: 11
            wrapMode: Text.WordWrap
        }
    }

    Repeater {
        model: Room.invites
        delegate: RowLayout {
            required property var modelData
            Layout.fillWidth: true
            spacing: 8
            Icon { name: "clock"; size: 14; color: K.muted }
            Text {
                Layout.fillWidth: true
                text: I18n.s.inviteUses.replace("{uses}", modelData.uses).replace("{max}", modelData.maxUses)
                      .replace("{date}", Qt.formatDateTime(modelData.expires, "dd.MM HH:mm"))
                color: K.text
                font.family: Theme.fontFamily
                font.pixelSize: 12
                elide: Text.ElideRight
            }
            KButton {
                small: true
                kind: "quiet"
                text: I18n.s.inviteRevoke
                enabled: !Room.moderating
                onClicked: Room.revokeInvite(modelData.id)
            }
        }
    }
}
