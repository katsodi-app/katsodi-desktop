import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Katsodi.App
import Katsodi.Ui

// Native sign-in with the website account, over the night sky. The
// password goes to Supabase Auth and is never stored by the app.
Item {
    id: screen
    signal done()

    Connections {
        target: Account
        function onChanged() { if (Account.signedIn) screen.done() }
    }

    Glass {
        anchors.centerIn: parent
        width: Math.min(480, parent.width - 40)
        implicitHeight: form.implicitHeight + 76
        strong: true
        radius: 28
        ColumnLayout {
            id: form
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 38
            spacing: 14

            Logo { size: 48; Layout.bottomMargin: 4 }
            PageHero {
                Layout.fillWidth: true
                title: I18n.s.signInDisplay
                accent: I18n.s.signInAccent
                lead: I18n.s.signInIntro
                size: 44
                spacing: 10
            }
            KTextField {
                id: email
                Layout.fillWidth: true
                Layout.topMargin: 8
                iconName: "globe"
                placeholderText: I18n.s.email
                inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                maximumLength: 320
                focus: true
                onAccepted: password.forceActiveFocus()
            }
            KTextField {
                id: password
                Layout.fillWidth: true
                iconName: "lock"
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
                font.family: Theme.fontFamily
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
            KButton {
                id: submit
                Layout.fillWidth: true
                Layout.topMargin: 6
                kind: "accent"
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
                color: K.muted
                font.family: Theme.fontFamily
                font.pixelSize: 12
                wrapMode: Text.WordWrap
            }
        }
    }
}
