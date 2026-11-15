import QtQuick 2.14

Item {
    id: card

    property string title: ""
    property string subtitle: ""
    property string library: ""
    property string image: ""
    property real progress: 0
    property bool played: false
    property bool active: false
    // card 16:9 per le righe "Continua a guardare" / "Prossimo episodio"
    property bool wide: false

    width: 150
    height: wide ? 250 : 278

    // alone dell'elemento selezionato
    Rectangle {
        anchors.fill: frame
        anchors.margins: -5
        radius: 12
        color: "#2e00a4dc"
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
            color: "#1a2430"
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
            color: "#5b6b7c"
            font.pixelSize: 14
            text: card.title
            visible: card.image.length === 0
        }

        // avanzamento della visione
        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            height: 5
            radius: 3
            color: "#66000000"
            visible: card.progress > 0

            Rectangle {
                height: parent.height
                radius: 3
                width: parent.width * Math.min(1, card.progress / 100.0)
                color: "#00a4dc"
            }
        }

        // badge "visto"
        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.margins: 9
            width: 50
            height: 20
            radius: 5
            color: "#dd0b0f14"
            border.width: 1
            border.color: "#2b6f8c"
            visible: card.played

            Text {
                anchors.centerIn: parent
                color: "#7fd9ff"
                font.pixelSize: 11
                font.bold: true
                text: qsTr("VISTO")
            }
        }

        // bordo di selezione, SOPRA l'immagine
        Rectangle {
            anchors.fill: parent
            radius: 8
            color: "transparent"
            border.width: card.active ? 3 : 1
            border.color: card.active ? "#00a4dc" : "#243040"
        }
    }

    Text {
        anchors.top: frame.bottom
        anchors.topMargin: 6
        width: parent.width
        height: card.wide ? 40 : 42
        color: card.active ? "#ffffff" : "#c9d4df"
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
        color: "#6f8090"
        font.pixelSize: 14
        elide: Text.ElideRight
        text: card.subtitle
        visible: card.subtitle.length > 0
    }

    // da quale libreria arriva (per distinguere doppioni e serie)
    Text {
        anchors.top: frame.bottom
        anchors.topMargin: 70
        width: parent.width
        color: "#5f7c8f"
        font.pixelSize: 12
        elide: Text.ElideRight
        text: card.library
        visible: card.library.length > 0
    }
}
