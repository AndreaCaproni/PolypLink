import QtQuick 2.14
import "."

// Full synopsis in full screen.
Item {
    id: overviewOverlay

    property var detail: ({})

    Rectangle {
        anchors.fill: parent
        color: Theme.overlay
    }

    Column {
        anchors.fill: parent
        anchors.margins: 60
        spacing: 16

        Text {
            width: parent.width
            color: Theme.text
            font.pixelSize: 28
            font.bold: true
            elide: Text.ElideRight
            text: overviewOverlay.detail.name !== undefined
                  ? overviewOverlay.detail.name : ""
        }

        Flickable {
            id: flick
            width: parent.width
            height: parent.height - 40
            contentWidth: width
            contentHeight: fullText.height
            clip: true
            boundsBehavior: Flickable.StopAtBounds

            Text {
                id: fullText
                width: flick.width
                color: Theme.textSoft
                font.pixelSize: 18
                lineHeight: 1.25
                wrapMode: Text.WordWrap
                text: overviewOverlay.detail.overview !== undefined
                      ? overviewOverlay.detail.overview : ""
            }
        }
    }

    function scrollBy(delta) {
        flick.contentY = Math.max(0, Math.min(
            Math.max(0, flick.contentHeight - flick.height),
            flick.contentY + delta))
    }
}
