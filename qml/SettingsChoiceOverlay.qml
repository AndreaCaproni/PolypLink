import QtQuick 2.14
import "."

// Popup condiviso per la scelta di un valore nelle Impostazioni.
Item {
    id: choiceOverlay

    property string title: ""
    property string action: ""
    property var items: []
    property int index: 0
    property int audioDelay: 0

    Rectangle {
        anchors.fill: parent
        color: Theme.overlay
    }

    Column {
        anchors.centerIn: parent
        spacing: 14

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.text
            font.pixelSize: 28
            font.bold: true
            text: choiceOverlay.title
        }

        Rectangle {
            visible: choiceOverlay.action !== "audiodelay"
            width: 520
            height: Math.min(460, 54 + choiceOverlay.items.length * 42)
            radius: 14
            color: Theme.overlayPanel
            border.width: 2
            border.color: Theme.accent

            ListView {
                id: list
                anchors.fill: parent
                anchors.margins: 10
                clip: true
                spacing: 2
                model: choiceOverlay.items
                boundsBehavior: Flickable.StopAtBounds

                onVisibleChanged: {
                    if (visible) positionViewAtIndex(choiceOverlay.index, ListView.Contain)
                }

                delegate: Rectangle {
                    width: list.width
                    height: 40
                    radius: 8
                    color: index === choiceOverlay.index ? Theme.selected : "transparent"
                    border.width: index === choiceOverlay.index ? 2 : 0
                    border.color: Theme.accent

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 28
                        anchors.verticalCenter: parent.verticalCenter
                        color: index === choiceOverlay.index ? Theme.text : Theme.textSoft
                        font.pixelSize: 16
                        font.bold: index === choiceOverlay.index
                        elide: Text.ElideRight
                        text: modelData.label
                    }

                    Rectangle {
                        visible: modelData.selected
                        width: 8
                        height: 8
                        radius: 4
                        color: Theme.accentSoft
                        anchors.right: parent.right
                        anchors.rightMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }
        }

        Rectangle {
            visible: choiceOverlay.action === "audiodelay"
            width: 520
            height: 110
            radius: 14
            color: Theme.overlayPanel
            border.width: 2
            border.color: Theme.accent

            Row {
                anchors.centerIn: parent
                spacing: 28

                Text {
                    color: Theme.accentSoft
                    font.pixelSize: 30
                    font.bold: true
                    text: "<"
                }

                Text {
                    width: 160
                    horizontalAlignment: Text.AlignHCenter
                    color: Theme.text
                    font.pixelSize: 28
                    font.bold: true
                    text: (choiceOverlay.audioDelay > 0 ? "+" : "") +
                          choiceOverlay.audioDelay + " ms"
                }

                Text {
                    color: Theme.accentSoft
                    font.pixelSize: 30
                    font.bold: true
                    text: ">"
                }
            }
        }
    }

    function ensureVisible(i) {
        list.positionViewAtIndex(i, ListView.Contain)
    }
}
