import QtQuick 2.14
import "."

// Quick Connect code: the user enters it from the Jellyfin panel.
Item {
    property string code: ""
    property string status: ""

    Column {
        anchors.centerIn: parent
        width: 760
        spacing: 14

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.text
            font.pixelSize: 28
            font.bold: true
            text: qsTr("Collega questo dispositivo")
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.accent
            font.pixelSize: 88
            font.bold: true
            font.letterSpacing: 12
            text: code.length > 0 ? code : "······"
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textMuted
            font.pixelSize: 18
            wrapMode: Text.WordWrap
            text: status
        }

    }
}
