import QtQuick 2.14
import "."

// Page for a season: header, actions and episode list.
Item {
    id: episodesPage

    property var seasonInfo: ({})
    property string seasonTitle: ""
    property string seriesTitle: ""
    property string subtitleText: ""
    property var episodes: []
    property string seriesPoster: ""
    property bool seasonActionsFocus: false
    property string seasonAction: "play"
    property int epIndex: 0
    property bool loading: false

    property string watchFromStartLabel: ""
    property string watchedLabel: ""
    property string unwatchedLabel: ""
    property string favoriteLabel: ""
    property string unfavoriteLabel: ""
    property string seenLabel: ""
    property string emptyLabel: ""

    Row {
        id: seasonDetailHeader
        anchors.top: parent.top
        anchors.topMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 20

        RoundedImage {
            width: 110
            height: 190
            radius: 8
            source: (episodesPage.seasonInfo.image !== undefined &&
                     episodesPage.seasonInfo.image.length > 0)
                    ? episodesPage.seasonInfo.image : episodesPage.seriesPoster
            sourceSizeWidth: 300
        }

        Column {
            width: parent.width - 130
            spacing: 8

            Text {
                width: parent.width
                color: Theme.text
                font.pixelSize: 26
                font.bold: true
                elide: Text.ElideRight
                text: episodesPage.seasonTitle
            }

            Text {
                width: parent.width
                color: Theme.textDim
                font.pixelSize: 15
                elide: Text.ElideRight
                text: episodesPage.subtitleText
            }

            Row {
                spacing: 10

                ActionButton {
                    icon: "replay"
                    label: episodesPage.watchFromStartLabel
                    primary: true
                    active: episodesPage.seasonActionsFocus && episodesPage.seasonAction === "play"
                }
                ActionButton {
                    icon: (episodesPage.seasonInfo.played !== undefined &&
                           episodesPage.seasonInfo.played) ? "check-double" : "check"
                    label: (episodesPage.seasonInfo.played !== undefined &&
                            episodesPage.seasonInfo.played)
                           ? episodesPage.unwatchedLabel : episodesPage.watchedLabel
                    active: episodesPage.seasonActionsFocus && episodesPage.seasonAction === "watched"
                }
                ActionButton {
                    icon: (episodesPage.seasonInfo.favorite !== undefined &&
                           episodesPage.seasonInfo.favorite) ? "heart-filled" : "heart-empty"
                    customIconColor: (episodesPage.seasonInfo.favorite !== undefined &&
                                      episodesPage.seasonInfo.favorite)
                                     ? Theme.favorite : Theme.text
                    label: (episodesPage.seasonInfo.favorite !== undefined &&
                            episodesPage.seasonInfo.favorite)
                           ? episodesPage.unfavoriteLabel : episodesPage.favoriteLabel
                    active: episodesPage.seasonActionsFocus && episodesPage.seasonAction === "favorite"
                }
            }

            Text {
                width: parent.width
                height: 40
                color: Theme.textSubtle
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: episodesPage.seasonInfo.overview !== undefined
                      ? episodesPage.seasonInfo.overview : ""
            }
        }
    }

    ListView {
        id: episodeList
        anchors.top: seasonDetailHeader.bottom
        anchors.topMargin: 10
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        clip: true
        model: episodesPage.episodes
        spacing: 8
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            width: episodeList.width
            height: 112

            Rectangle {
                anchors.fill: parent
                radius: 8
                color: index === episodesPage.epIndex && !episodesPage.seasonActionsFocus
                       ? Theme.selected : Theme.panelAlt
                border.width: index === episodesPage.epIndex && !episodesPage.seasonActionsFocus
                              ? 2 : 1
                border.color: index === episodesPage.epIndex && !episodesPage.seasonActionsFocus
                              ? Theme.accent : Theme.borderDark
            }

            RoundedImage {
                x: 10
                anchors.verticalCenter: parent.verticalCenter
                width: 150
                height: 84
                radius: 7
                source: modelData.image !== undefined ? modelData.image : ""
                sourceSizeWidth: 300
            }

            Text {
                id: epRowTitle
                anchors.left: parent.left
                anchors.leftMargin: 174
                anchors.right: parent.right
                anchors.rightMargin: 110
                anchors.top: parent.top
                anchors.topMargin: 12
                color: Theme.text
                font.pixelSize: 19
                font.bold: index === episodesPage.epIndex && !episodesPage.seasonActionsFocus
                elide: Text.ElideRight
                text: {
                    var n = modelData.indexNumber !== undefined ? modelData.indexNumber : 0
                    var sn = modelData.parentIndexNumber !== undefined
                             ? modelData.parentIndexNumber : 0
                    var label = (sn > 0 || n > 0) ? ("S" + sn + " E" + n + "  ·  ") : ""
                    return label + (modelData.name !== undefined ? modelData.name : "")
                }
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 174
                anchors.right: parent.right
                anchors.rightMargin: 110
                anchors.top: epRowTitle.bottom
                anchors.topMargin: 3
                color: Theme.textSubtle
                font.pixelSize: 14
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                text: modelData.overview !== undefined ? modelData.overview : ""
            }

            Rectangle {
                anchors.right: parent.right
                anchors.rightMargin: 14
                anchors.verticalCenter: parent.verticalCenter
                width: 78
                height: 22
                radius: 4
                color: Theme.bg
                border.width: 1
                border.color: Theme.borderAccent
                visible: modelData.played !== undefined && modelData.played
                Text {
                    anchors.centerIn: parent
                    color: Theme.accentSoft
                    font.pixelSize: 12
                    text: episodesPage.seenLabel
                }
            }

            Rectangle {
                anchors.left: parent.left
                anchors.leftMargin: 174
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 8
                width: 220
                height: 4
                radius: 2
                color: Theme.borderDark
                visible: modelData.progress !== undefined && modelData.progress > 0

                Rectangle {
                    height: parent.height
                    radius: 2
                    width: parent.width * Math.min(1, (modelData.progress !== undefined
                                                       ? modelData.progress : 0) / 100.0)
                    color: Theme.accent
                }
            }
        }
    }

    EmptyState {
        visible: episodesPage.episodes.length === 0 && !episodesPage.loading
        label: episodesPage.emptyLabel
    }

    function ensureVisible(index) {
        episodeList.positionViewAtIndex(index, ListView.Contain)
    }
}
