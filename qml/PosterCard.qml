import QtQuick 2.14
import "."

Item {
    id: card

    property string title: ""
    property string subtitle: ""
    property string library: ""
    property string image: ""
    property real progress: 0
    property bool played: false
    property bool active: false
    // 16:9 card for the "Continue watching" / "Next episode" rows
    property bool wide: false

    width: 150
    height: wide ? 250 : 278

    // glow of the selected item
    Rectangle {
        anchors.fill: frame
        anchors.margins: -5
        radius: 12
        color: Theme.accentGlowSoft
        visible: card.active
        z: -1
    }

    Item {
        id: frame
        width: parent.width
        height: card.wide ? Math.round(card.width * 9 / 16) : 208

        Rectangle {
            anchors.fill: parent
            radius: 8
            color: Theme.borderDeep
        }

        RoundedImage {
            anchors.fill: parent
            radius: 8
            source: card.image
            sourceSizeWidth: 340
        }

        Text {
            anchors.centerIn: parent
            width: parent.width - 16
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
            color: Theme.textFaint
            font.pixelSize: 14
            text: card.title
            visible: card.image.length === 0
        }

        // watch progress
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            height: 5
            radius: 3
            color: Theme.overlayMedium
            visible: card.progress > 0

            Rectangle {
                height: parent.height
                radius: 3
                width: parent.width * Math.min(1, card.progress / 100.0)
                color: Theme.accent
            }
        }

        // "watched" badge
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 9
            width: 50
            height: 20
            radius: 5
            color: Theme.posterOverlay
            border.width: 1
            border.color: Theme.borderAccent
            visible: card.played

            Text {
                anchors.centerIn: parent
                color: Theme.accentSoft
                font.pixelSize: 11
                font.bold: true
                text: qsTr("VISTO")
            }
        }

        // selection border, ABOVE the image
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"
            border.width: card.active ? 3 : 1
            border.color: card.active ? Theme.accent : Theme.borderStrong
        }
    }

    Text {
        anchors.top: frame.bottom
        anchors.topMargin: 6
        width: parent.width
        height: card.wide ? 40 : 42
        color: card.active ? Theme.text : Theme.textSoft
        font.pixelSize: 17
        font.bold: card.active
        wrapMode: Text.WordWrap
        maximumLineCount: 2
        elide: Text.ElideRight
        text: card.title
    }

    Text {
        anchors.top: frame.bottom
        anchors.topMargin: 50
        width: parent.width
        color: Theme.textDim
        font.pixelSize: 14
        elide: Text.ElideRight
        text: card.subtitle
        visible: card.subtitle.length > 0
    }

    // which library it comes from (to tell duplicates and series apart)
    Text {
        anchors.top: frame.bottom
        anchors.topMargin: 70
        width: parent.width
        color: Theme.textSubtleAlt
        font.pixelSize: 12
        elide: Text.ElideRight
        text: card.library
        visible: card.library.length > 0
    }
}
