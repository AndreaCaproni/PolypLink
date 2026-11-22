import QtQuick 2.14
import "."

// Login: method selection (credentials or Quick Connect) and credential fields.
Item {
    property string serverLabel: ""
    property bool insecureHttp: false
    property string authStep: "menu"
    property int authIndex: 0
    property string credUser: ""
    property string credPass: ""

    function maskText(s) {
        var out = ""
        for (var i = 0; i < s.length; ++i) out += "\u2022"
        return out
    }

    Column {
        anchors.centerIn: parent
        width: 640
        spacing: 16

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.text
            font.pixelSize: 28
            font.bold: true
            text: qsTr("Accedi a %1").arg(serverLabel)
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textDim
            font.pixelSize: 17
            text: serverLabel.length > 0 ? "" : serverLabel
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.dangerSoft
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            visible: insecureHttp
            text: qsTr("Attenzione: connessione non cifrata. Password e token viaggiano in chiaro.")
        }

        // --- method selection menu ---
        Column {
            width: parent.width
            spacing: 12
            visible: authStep === "menu"

            Rectangle {
                width: parent.width
                height: 62
                radius: 8
                color: authIndex === 0 ? Theme.selected : Theme.panel
                border.width: authIndex === 0 ? 2 : 1
                border.color: authIndex === 0 ? Theme.accent : Theme.border
                Text {
                    anchors.centerIn: parent
                    color: Theme.text
                    font.pixelSize: 20
                    text: qsTr("Nome utente e password")
                }
            }

            Rectangle {
                width: parent.width
                height: 62
                radius: 8
                color: authIndex === 1 ? Theme.selected : Theme.panel
                border.width: authIndex === 1 ? 2 : 1
                border.color: authIndex === 1 ? Theme.accent : Theme.border
                Text {
                    anchors.centerIn: parent
                    color: Theme.text
                    font.pixelSize: 20
                    text: qsTr("Quick Connect (codice a 6 cifre)")
                }
            }

        }

        // --- credentials ---
        Column {
            width: parent.width
            spacing: 12
            visible: authStep === "credentials"

            Rectangle {
                width: parent.width
                height: 62
                radius: 8
                color: authIndex === 0 ? Theme.selected : Theme.panel
                border.width: authIndex === 0 ? 2 : 1
                border.color: authIndex === 0 ? Theme.accent : Theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    color: Theme.textDim
                    font.pixelSize: 14
                    text: qsTr("nome utente")
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    color: Theme.text
                    font.pixelSize: 20
                    text: credUser.length > 0 ? credUser : "—"
                }
            }

            Rectangle {
                width: parent.width
                height: 62
                radius: 8
                color: authIndex === 1 ? Theme.selected : Theme.panel
                border.width: authIndex === 1 ? 2 : 1
                border.color: authIndex === 1 ? Theme.accent : Theme.border

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.top: parent.top
                    anchors.topMargin: 8
                    color: Theme.textDim
                    font.pixelSize: 14
                    text: qsTr("password")
                }
                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 10
                    color: Theme.text
                    font.pixelSize: 20
                    text: credPass.length > 0 ? maskText(credPass) : "—"
                }
            }

            Rectangle {
                width: parent.width
                height: 52
                radius: 26
                color: authIndex === 2 ? Theme.accent : Theme.selected
                border.width: authIndex === 2 ? 2 : 1
                border.color: authIndex === 2 ? Theme.accentSoft : Theme.border

                Text {
                    anchors.centerIn: parent
                    color: authIndex === 2 ? Theme.accentText : Theme.text
                    font.pixelSize: 19
                    font.bold: true
                    text: qsTr("Entra")
                }
            }

        }
    }
}
