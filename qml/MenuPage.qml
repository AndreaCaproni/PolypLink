import QtQuick 2.14
import "."

// Pagina condivisa da Impostazioni e Account: elenco a sinistra, dettaglio a destra.
Item {
    id: menuPage

    property string title: ""
    property string subtitle: ""
    property var entries: []
    property int menuIndex: 0
    property bool accountMode: false
    property bool profileView: false
    property string profileName: ""
    property string profileImage: ""
    property string serverLabel: ""
    property var userStats: ({})
    property string currentTitle: ""
    property string currentValue: ""
    property string currentDesc: ""
    property bool comingSoon: false
    property string versionText: ""
    property var valueFor: null
    property string moviesLabel: ""
    property string seriesLabel: ""
    property string episodesLabel: ""
    property string favoritesLabel: ""
    property string comingSoonLabel: ""

    function valueAt(i) {
        if (!entries || i < 0 || i >= entries.length) return ""
        const entry = entries[i]
        if (!entry) return ""
        return valueFor ? valueFor(entry) : ""
    }

    function ensureVisible(index) {
        menuList.positionViewAtIndex(index, ListView.Contain)
    }



    onVisibleChanged: {
        if (visible) menuList.positionViewAtIndex(0, ListView.Beginning)
    }

    // --- colonna sinistra: elenco
    Text {
        x: 40
        y: 12
        color: Theme.text
        font.pixelSize: 30
        font.bold: true
        text: title
    }
    Text {
        x: 40
        y: 52
        width: 540
        color: Theme.textDim
        font.pixelSize: 14
        elide: Text.ElideRight
        text: subtitle
    }

    ListView {
        id: menuList
        x: 40
        y: 90
        width: 540
        height: parent.height - 126
        clip: true
        model: entries
        spacing: 4
        boundsBehavior: Flickable.StopAtBounds

        delegate: Rectangle {
            width: menuList.width
            height: 48
            radius: 8
            color: index === menuIndex
                   ? Theme.selected
                   : (index % 2 === 0 ? Theme.panelAlternate : Theme.bgAlt)
            border.width: index === menuIndex ? 2 : 0
            border.color: Theme.accent

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: parent.right
                anchors.rightMargin: valueAt(index).length > 0 ? 150 : 14
                anchors.verticalCenter: parent.verticalCenter
                color: index === menuIndex ? Theme.text : Theme.textSoft
                font.pixelSize: 16
                font.bold: index === menuIndex
                elide: Text.ElideRight
                text: entries[index] && entries[index].title ? entries[index].title : ""
            }

            Text {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.accentSoft
                font.pixelSize: 14
                visible: valueAt(index).length > 0
                text: valueAt(index)
            }
        }
    }

    // --- colonna destra: dettaglio della voce selezionata
    Rectangle {
        x: 620
        y: 16
        width: 620
        height: parent.height - 32
        radius: 16
        color: Theme.panelDeep
        border.width: 1
        border.color: Theme.borderSoft

        Rectangle {
            x: 28
            y: 34
            width: 4
            height: 54
            radius: 2
            color: Theme.accent
            visible: !accountMode
        }

        // profilo utente
        Row {
            x: 28
            y: 24
            spacing: 18
            visible: profileView

            Item {
                width: 84
                height: 84

                Rectangle {
                    anchors.fill: parent
                    radius: 42
                    color: Theme.chip
                    border.width: 1
                    border.color: Theme.borderStrong
                }
                Text {
                    anchors.centerIn: parent
                    color: Theme.textSoft
                    font.pixelSize: 34
                    font.bold: true
                    text: profileName.length > 0 ? profileName.charAt(0).toUpperCase() : "?"
                }
                RoundedImage {
                    anchors.fill: parent
                    radius: 42
                    source: profileImage
                    sourceSizeWidth: 168
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4
                Text {
                    color: Theme.text
                    font.pixelSize: 24
                    font.bold: true
                    text: profileName
                }
                Text {
                    color: Theme.textDim
                    font.pixelSize: 14
                    text: serverLabel
                }
            }
        }

        // statistiche
        Row {
            x: 28
            y: 124
            spacing: 10
            visible: profileView

            Rectangle {
                width: 130; height: 64; radius: 10
                color: Theme.panelAlternate; border.width: 1; border.color: Theme.borderSoft
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.accentSoft; font.pixelSize: 20; font.bold: true
                           text: userStats.movies !== undefined ? userStats.movies : "–" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.textDim; font.pixelSize: 12; text: moviesLabel }
                }
            }
            Rectangle {
                width: 130; height: 64; radius: 10
                color: Theme.panelAlternate; border.width: 1; border.color: Theme.borderSoft
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.accentSoft; font.pixelSize: 20; font.bold: true
                           text: userStats.series !== undefined ? userStats.series : "–" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.textDim; font.pixelSize: 12; text: seriesLabel }
                }
            }
            Rectangle {
                width: 130; height: 64; radius: 10
                color: Theme.panelAlternate; border.width: 1; border.color: Theme.borderSoft
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.accentSoft; font.pixelSize: 20; font.bold: true
                           text: userStats.episodes !== undefined ? userStats.episodes : "–" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.textDim; font.pixelSize: 12; text: episodesLabel }
                }
            }
            Rectangle {
                width: 130; height: 64; radius: 10
                color: Theme.panelAlternate; border.width: 1; border.color: Theme.borderSoft
                Column {
                    anchors.centerIn: parent
                    spacing: 2
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.accentSoft; font.pixelSize: 20; font.bold: true
                           text: userStats.favorites !== undefined ? userStats.favorites : "–" }
                    Text { anchors.horizontalCenter: parent.horizontalCenter
                           color: Theme.textDim; font.pixelSize: 12; text: favoritesLabel }
                }
            }
        }

        Rectangle {
            x: 28
            y: 202
            width: parent.width - 56
            height: 1
            color: Theme.borderSoft
            visible: profileView
        }

        Column {
            x: 48
            y: 28
            width: parent.width - 88
            spacing: 10
            visible: !profileView

            Text {
                width: parent.width
                color: Theme.text
                font.pixelSize: 28
                font.bold: true
                wrapMode: Text.WordWrap
                text: currentTitle
            }
            Text {
                width: parent.width
                color: Theme.accentSoft
                font.pixelSize: 22
                font.bold: true
                visible: currentValue.length > 0
                text: currentValue
            }
            Text {
                width: parent.width
                color: Theme.textSubtle
                font.pixelSize: 15
                lineHeight: 1.2
                wrapMode: Text.WordWrap
                text: currentDesc
            }
            Text {
                width: parent.width
                color: Theme.accentSoft
                font.pixelSize: 14
                font.bold: true
                visible: comingSoon
                text: comingSoonLabel
            }
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 28
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 22
            color: Theme.textFaint
            font.pixelSize: 13
            text: versionText
        }
    }
}
