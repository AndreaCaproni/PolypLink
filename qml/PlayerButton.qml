import QtQuick 2.14

// Pulsante dell'OSD del player. Pillola come il resto della UI, con lo stesso
// linguaggio visivo dei comandi del client Jellyfin.
Rectangle {
    id: button

    property string text: ""
    property bool active: false
    property bool primary: false

    width: Math.max(92, label.implicitWidth + 36)
    height: 44
    radius: 22
    color: active ? "#1d5a75" : (primary ? "#00a4dc" : "#101920")
    border.width: active ? 2 : 1
    border.color: active ? "#7fd9ff" : (primary ? "#7fd9ff" : "#1e2b38")

    Text {
        id: label
        anchors.centerIn: parent
        color: button.primary && !button.active ? "#00121a" : "#ffffff"
        font.pixelSize: 15
        font.bold: button.active || button.primary
        text: button.text
    }
}
