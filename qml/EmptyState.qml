import QtQuick 2.14
import "."

// Centered message for empty libraries, seasons and episodes.
Text {
    property string label: ""

    anchors.centerIn: parent
    horizontalAlignment: Text.AlignHCenter
    color: Theme.textDim
    font.pixelSize: 18
    text: label
}
