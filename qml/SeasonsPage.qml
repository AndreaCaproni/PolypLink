import QtQuick 2.14
import "."

// Pagina delle stagioni di una serie.
Item {
    id: seasonsPage

    property string seriesTitle: ""
    property var seasons: []
    property string seriesPoster: ""
    property int seasonIndex: 0
    property string countText: ""
    property string emptyLabel: ""
    property var episodeCountText: null
    property bool loading: false

    readonly property int columns: Math.max(1, Math.floor(grid.width / grid.cellWidth))

    Text {
        id: header
        x: 24
        anchors.top: parent.top
        anchors.topMargin: 10
        color: Theme.text
        font.pixelSize: 26
        font.bold: true
        text: seasonsPage.seriesTitle
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 24
        anchors.top: header.bottom
        anchors.topMargin: 2
        color: Theme.textDim
        font.pixelSize: 15
        text: seasonsPage.countText
    }

    GridView {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 34
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        clip: true
        cellWidth: 152
        cellHeight: 292
        boundsBehavior: Flickable.StopAtBounds
        model: seasonsPage.seasons

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            PosterCard {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 150
                title: modelData.name !== undefined ? modelData.name : ""
                subtitle: seasonsPage.episodeCountText
                          ? seasonsPage.episodeCountText(modelData.childCount !== undefined
                                                         ? modelData.childCount : 0)
                          : ""
                image: (modelData.image !== undefined && modelData.image.length > 0)
                       ? modelData.image : seasonsPage.seriesPoster
                played: modelData.played !== undefined ? modelData.played : false
                active: index === seasonsPage.seasonIndex
            }
        }
    }

    EmptyState {
        visible: seasonsPage.seasons.length === 0 && !seasonsPage.loading
        label: seasonsPage.emptyLabel
    }

    function ensureVisible(index) {
        grid.positionViewAtIndex(index, GridView.Contain)
    }
}
