import QtQuick 2.14
import "."

// Messaggio centrato per librerie, stagioni ed episodi vuoti.
Text {
    property string label: ""

    anchors.centerIn: parent
    horizontalAlignment: Text.AlignHCenter
    color: Theme.textDim
    font.pixelSize: 18
    text: label
}
