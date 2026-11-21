import QtQuick 2.14
import "."

// Selettore della lingua dell'interfaccia.
Item {
    id: languageOverlay

    property string title: ""
    property var languages: []
    property int currentIndex: 0

    Rectangle {
        anchors.fill: parent
        color: Theme.overlay
    }

    Column {
        anchors.centerIn: parent
        spacing: 18

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.text
            font.pixelSize: 28
            font.bold: true
            text: languageOverlay.title
        }

        GridView {
            id: grid
            width: 1110
            height: 454
            cellWidth: 370
            cellHeight: 62
            interactive: false
            clip: true
            model: languageOverlay.languages

            delegate: Item {
                width: grid.cellWidth
                height: grid.cellHeight

                Rectangle {
                    width: 350
                    height: 54
                    anchors.centerIn: parent
                    radius: 27
                    color: index === languageOverlay.currentIndex ? Theme.selected : Theme.panel
                    border.width: index === languageOverlay.currentIndex ? 2 : 1
                    border.color: index === languageOverlay.currentIndex ? Theme.accent : Theme.border

                    Text {
                        anchors.centerIn: parent
                        color: index === languageOverlay.currentIndex ? Theme.text : Theme.textSoft
                        font.pixelSize: 19
                        font.bold: index === languageOverlay.currentIndex
                        text: modelData.name
                    }
                }
            }
        }
    }
}
