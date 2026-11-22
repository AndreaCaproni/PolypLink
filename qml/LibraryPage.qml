import QtQuick 2.14
import "."

// Grid of a library or of favorites.
Item {
    id: libraryPage

    property string title: ""
    property var items: []
    property int gridIndex: 0
    property bool navFocus: false
    property string countText: ""
    property string emptyLabel: ""
    property bool loading: false

    readonly property int columns: Math.max(1, Math.floor(grid.width / grid.cellWidth))

    Text {
        id: header
        x: 24
        anchors.top: parent.top
        anchors.topMargin: 10
        color: Theme.text
        font.pixelSize: 28
        font.bold: true
        text: libraryPage.title
    }

    Text {
        anchors.right: parent.right
        anchors.rightMargin: 24
        anchors.top: parent.top
        anchors.topMargin: 16
        color: Theme.textFaint
        font.pixelSize: 15
        text: libraryPage.countText
    }

    GridView {
        id: grid
        anchors.top: header.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        cellWidth: 152
        cellHeight: 292
        clip: true
        model: libraryPage.items
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            width: grid.cellWidth
            height: grid.cellHeight

            PosterCard {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 150
                title: modelData.name !== undefined ? modelData.name : ""
                subtitle: modelData.subtitle !== undefined ? modelData.subtitle : ""
                image: modelData.image !== undefined ? modelData.image : ""
                progress: modelData.progress !== undefined ? modelData.progress : 0
                active: index === libraryPage.gridIndex && !libraryPage.navFocus
            }
        }
    }

    EmptyState {
        visible: libraryPage.items.length === 0 && !libraryPage.loading
        label: libraryPage.emptyLabel
    }

    function ensureVisible(index) {
        grid.positionViewAtIndex(index, GridView.Contain)
    }
}
