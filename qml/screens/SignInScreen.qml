import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import "../controls"

// Native sign-in with the same account as the website. The password is
// handed to Supabase Auth and never stored by the application.
Item {
    id: screen
    signal done()

    Connections {
        target: Account
        function onChanged() { if (Account.signedIn) screen.done() }
    }

    Rectangle {
        anchors.centerIn: parent
        width: Math.min(440, parent.width - 32)
        implicitHeight: form.implicitHeight + 72
        radius: 16
        color: Theme.c.surface

        ColumnLayout {
            id: form
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 36
            spacing: 14

            Text {
                text: I18n.s.signInTitle
                color: Theme.c.foreground
                font.pixelSize: 34
                font.weight: Font.DemiBold
                font.letterSpacing: -2
                Accessible.role: Accessible.Heading
            }
            Text {
                Layout.fillWidth: true
                text: I18n.s.signInIntro
                color: Theme.c.mutedForeground
                font.pixelSize: 14
                wrapMode: Text.WordWrap
            }
            Text { text: I18n.s.email; color: Theme.c.foreground; font.pixelSize: 12; font.weight: Font.DemiBold; Layout.topMargin: 6 }
            KTextField {
                id: email
                Layout.fillWidth: true
                placeholderText: I18n.s.email
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                maximumLength: 320
                focus: true
                onAccepted: password.forceActiveFocus()
            }
            Text { text: I18n.s.password; color: Theme.c.foreground; font.pixelSize: 12; font.weight: Font.DemiBold }
            KTextField {
                id: password
                Layout.fillWidth: true
                placeholderText: I18n.s.password
                echoMode: TextInput.Password
                maximumLength: 128
                onAccepted: submit.clicked()
            }
            Text {
                Layout.fillWidth: true
                visible: Account.errorKey !== "" && Account.state !== "authenticating"
                text: I18n.s[Account.errorKey] || I18n.s.errorServer
                color: Theme.c.danger
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
            KButton {
                id: submit
                Layout.fillWidth: true
                Layout.topMargin: 6
                text: Account.state === "authenticating" ? I18n.s.signInWorking : I18n.s.signIn
                enabled: Account.state !== "authenticating" && email.text.length > 0 && password.text.length > 0
                onClicked: {
                    Account.signIn(email.text, password.text)
                    password.clear() // not kept in the UI after use
                }
            }
            Text {
                Layout.fillWidth: true
                visible: !Account.persistent
                text: I18n.s.accountSessionOnly
                color: Theme.c.mutedForeground
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
