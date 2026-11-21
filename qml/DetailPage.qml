import QtQuick 2.14
import "."

// Scheda dettaglio di film, serie o episodio.
Item {
    id: detailPage

    property var detail: ({})
    property int detailFocus: 0

    property bool isSeries: false
    property bool hasResume: false
    property bool hasSeriesLink: false
    property bool isEpisode: false
    property bool hasOverview: false

    property int playedIndex: -1
    property int favoriteIndex: -1
    property int seriesIndex: -1
    property int previousIndex: -1
    property int readMoreIndex: -1
    property int settingsStart: -1

    property string qualityText: ""
    property string audioText: ""
    property string subtitleText: ""
    property string metaText: ""

    property string playLabel: ""
    property string seasonsLabel: ""
    property string restartLabel: ""
    property string watchedLabel: ""
    property string unwatchedLabel: ""
    property string favoriteLabel: ""
    property string unfavoriteLabel: ""
    property string seriesLabel: ""
    property string previousLabel: ""
    property string readMoreLabel: ""
    property string qualityLabel: ""
    property string audioLabelText: ""
    property string subtitleLabelText: ""

    Image {
        anchors.fill: parent
        source: detailPage.detail.backdrop !== undefined ? detailPage.detail.backdrop : ""
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
        cache: true
        visible: status === Image.Ready
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.overlayLight }
            GradientStop { position: 0.55; color: Theme.overlayMenu }
            GradientStop { position: 1.0; color: Theme.overlay }
        }
    }

    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 48
        anchors.rightMargin: 48
        anchors.bottomMargin: 40
        spacing: 28

        PosterCard {
            width: 150
            height: 278
            title: ""
            subtitle: ""
            image: detailPage.detail.image !== undefined ? detailPage.detail.image : ""
            progress: detailPage.detail.progress !== undefined ? detailPage.detail.progress : 0
            played: detailPage.detail.played !== undefined ? detailPage.detail.played : false
            active: false
        }

        Column {
            width: parent.width - 200
            spacing: 10

            Text {
                width: parent.width
                color: Theme.text
                font.pixelSize: 30
                font.bold: true
                elide: Text.ElideRight
                text: detailPage.detail.name !== undefined ? detailPage.detail.name : ""
            }

            Text {
                width: parent.width
                color: Theme.textMuted
                font.pixelSize: 15
                text: {
                    var parts = []
                    if (detailPage.detail.subtitle !== undefined &&
                        detailPage.detail.subtitle.length > 0)
                        parts.push(detailPage.detail.subtitle)
                    if (detailPage.detail.officialRating !== undefined &&
                        detailPage.detail.officialRating.length > 0)
                        parts.push(detailPage.detail.officialRating)
                    if (detailPage.detail.communityRating !== undefined &&
                        detailPage.detail.communityRating > 0)
                        parts.push("★ " + detailPage.detail.communityRating.toFixed(1))
                    return parts.join("   ·   ")
                }
            }

            // azioni: solo icona, si allargano con il focus
            Row {
                spacing: 10

                ActionButton {
                    icon: "play"
                    label: detailPage.playLabel
                    active: detailPage.detailFocus === 0
                    primary: true
                }
                ActionButton {
                    icon: "series"
                    label: detailPage.seasonsLabel
                    visible: detailPage.isSeries
                    active: detailPage.detailFocus === 1
                }
                ActionButton {
                    icon: "replay"
                    label: detailPage.restartLabel
                    visible: detailPage.hasResume && !detailPage.isSeries
                    active: detailPage.detailFocus === 1
                }
                ActionButton {
                    icon: (detailPage.detail.played !== undefined && detailPage.detail.played)
                          ? "check-double" : "check"
                    label: (detailPage.detail.played !== undefined && detailPage.detail.played)
                           ? detailPage.unwatchedLabel : detailPage.watchedLabel
                    active: detailPage.detailFocus === detailPage.playedIndex
                }
                ActionButton {
                    icon: (detailPage.detail.favorite !== undefined && detailPage.detail.favorite)
                          ? "heart-filled" : "heart-empty"
                    customIconColor: (detailPage.detail.favorite !== undefined &&
                                      detailPage.detail.favorite)
                                     ? Theme.favorite : Theme.text
                    label: (detailPage.detail.favorite !== undefined && detailPage.detail.favorite)
                           ? detailPage.unfavoriteLabel : detailPage.favoriteLabel
                    active: detailPage.detailFocus === detailPage.favoriteIndex
                }
                ActionButton {
                    icon: "series"
                    label: detailPage.seriesLabel
                    visible: detailPage.hasSeriesLink
                    active: detailPage.detailFocus === detailPage.seriesIndex
                }
                ActionButton {
                    icon: "previous"
                    label: detailPage.previousLabel
                    visible: detailPage.isEpisode
                    active: detailPage.detailFocus === detailPage.previousIndex
                }
                ActionButton {
                    icon: "text"
                    label: detailPage.readMoreLabel
                    visible: detailPage.hasOverview
                    active: detailPage.detailFocus === detailPage.readMoreIndex
                }
            }

            Text {
                width: parent.width
                height: 78
                color: Theme.textSoft
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                elide: Text.ElideRight
                text: detailPage.detail.overview !== undefined ? detailPage.detail.overview : ""
            }

            // impostazioni di riproduzione
            Column {
                spacing: 8
                visible: !detailPage.isSeries

                Rectangle {
                    width: 520; height: 40; radius: 8
                    color: detailPage.detailFocus === detailPage.settingsStart
                           ? Theme.selected : Theme.panelAlt
                    border.width: 1
                    border.color: detailPage.detailFocus === detailPage.settingsStart
                                  ? Theme.accent : Theme.borderDark
                    Text { anchors.left: parent.left; anchors.leftMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.textMuted; font.pixelSize: 15
                           text: detailPage.qualityLabel }
                    Text { anchors.right: parent.right; anchors.rightMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.text; font.pixelSize: 15
                           text: detailPage.qualityText }
                }

                Rectangle {
                    width: 520; height: 40; radius: 8
                    color: detailPage.detailFocus === detailPage.settingsStart + 1
                           ? Theme.selected : Theme.panelAlt
                    border.width: 1
                    border.color: detailPage.detailFocus === detailPage.settingsStart + 1
                                  ? Theme.accent : Theme.borderDark
                    Text { anchors.left: parent.left; anchors.leftMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.textMuted; font.pixelSize: 15
                           text: detailPage.audioLabelText }
                    Text { anchors.right: parent.right; anchors.rightMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.text; font.pixelSize: 15
                           text: detailPage.audioText }
                }

                Rectangle {
                    width: 520; height: 40; radius: 8
                    color: detailPage.detailFocus === detailPage.settingsStart + 2
                           ? Theme.selected : Theme.panelAlt
                    border.width: 1
                    border.color: detailPage.detailFocus === detailPage.settingsStart + 2
                                  ? Theme.accent : Theme.borderDark
                    Text { anchors.left: parent.left; anchors.leftMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.textMuted; font.pixelSize: 15
                           text: detailPage.subtitleLabelText }
                    Text { anchors.right: parent.right; anchors.rightMargin: 14
                           anchors.verticalCenter: parent.verticalCenter
                           color: Theme.text; font.pixelSize: 15
                           text: detailPage.subtitleText }
                }
            }

            Text {
                width: parent.width
                color: Theme.textDim
                font.pixelSize: 13
                elide: Text.ElideRight
                text: detailPage.metaText
            }
        }
    }
}
