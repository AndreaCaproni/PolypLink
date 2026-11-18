import QtQuick 2.14
import "."

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
    color: active ? Theme.primaryActive : (primary ? Theme.accent : Theme.panelAlt)
    border.width: active ? 2 : 1
    border.color: active ? Theme.accentSoft : (primary ? Theme.accentSoft : Theme.border)

    Text {
        id: label
        anchors.centerIn: parent
        color: button.primary && !button.active ? Theme.accentText : Theme.text
        font.pixelSize: 15
        font.bold: button.active || button.primary
        text: button.text
    }
}
