import QtQuick 2.14
import "."

// Scelta del server Jellyfin: server salvato, server trovati in rete o URL manuale.
Item {
    property var servers: []
    property bool searching: false
    property string serverLabel: ""
    property int selectedIndex: 0

    function savedServerRow() {
        return serverLabel.length > 0 ? 1 : 0
    }

    Column {
        anchors.centerIn: parent
        width: 640
        spacing: 16

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.text
            font.pixelSize: 30
            font.bold: true
            text: qsTr("Scegli il tuo server Jellyfin")
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textDim
            font.pixelSize: 17
            wrapMode: Text.WordWrap
            text: searching
                  ? qsTr("Ricerca sulla rete locale…")
                  : (servers.length === 0
                     ? qsTr("Nessun server rilevato automaticamente.")
                     : qsTr("Premi A per connetterti."))
        }

        // server già salvato: si rientra senza ridigitare l'indirizzo
        Rectangle {
            width: parent.width
            height: 62
            radius: 8
            visible: savedServerRow() === 1
            color: selectedIndex === 0 ? Theme.selected : Theme.panel
            border.width: selectedIndex === 0 ? 2 : 1
            border.color: selectedIndex === 0 ? Theme.accent : Theme.border

            Column {
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Text { color: Theme.text; font.pixelSize: 20; text: qsTr("Server salvato") }
                Text { color: Theme.textDim; font.pixelSize: 15; text: serverLabel }
            }
        }

        Repeater {
            model: servers
            delegate: Rectangle {
                width: parent.width
                height: 62
                radius: 8
                color: index + savedServerRow() === selectedIndex ? Theme.selected : Theme.panel
                border.width: index + savedServerRow() === selectedIndex ? 2 : 1
                border.color: index + savedServerRow() === selectedIndex ? Theme.accent : Theme.border

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { color: Theme.text; font.pixelSize: 20; text: modelData.name }
                    Text { color: Theme.textDim; font.pixelSize: 15; text: modelData.address }
                }
            }
        }

        Rectangle {
            width: parent.width
            height: 52
            radius: 8
            color: selectedIndex === servers.length + savedServerRow() ? Theme.selected : "transparent"
            border.width: 1
            border.color: selectedIndex === servers.length + savedServerRow() ? Theme.accent : Theme.border

            Text {
                anchors.centerIn: parent
                color: Theme.textMuted
                font.pixelSize: 17
                text: qsTr("Inserisci l'indirizzo del server…")
            }
        }

        Text {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            color: Theme.textFaint
            font.pixelSize: 15
            wrapMode: Text.WordWrap
            text: qsTr("A: seleziona    X/R: cerca in rete (LAN + UDP)    B: esci\nL'indirizzo si può anche mettere in\nsteamlink/config/system/jellyfin-server.txt sulla chiavetta")
        }
    }
}
