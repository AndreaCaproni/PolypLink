import QtQuick 2.14

Item {
    id: row

    property string title: ""
    property var items: []
    property int currentIndex: 0
    property bool active: false
    property bool landscape: false
    property bool showLibrary: false

    height: items.length > 0 ? (landscape ? 300 : (showLibrary ? 356 : 340)) : 0

    Text {
        id: titleText
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.leftMargin: 24
        color: row.active ? "#ffffff" : "#93a4b5"
        font.pixelSize: 24
        font.bold: true
        text: row.title
        visible: row.items.length > 0
    }

    ListView {
        id: list
        anchors.top: titleText.bottom
        anchors.topMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        height: row.landscape ? 252 : (row.showLibrary ? 306 : 292)
        orientation: ListView.Horizontal
        model: row.items
        spacing: 16
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        delegate: PosterCard {
            wide: row.landscape
            width: row.landscape ? 320 : 150
            height: row.landscape ? 250 : (row.showLibrary ? 296 : 278)
            title: row.landscape
                   ? (modelData.cardTitle !== undefined ? modelData.cardTitle : "")
                   : (modelData.name !== undefined ? modelData.name : "")
            subtitle: row.landscape
                      ? (modelData.cardSubtitle !== undefined ? modelData.cardSubtitle : "")
                      : (modelData.subtitle !== undefined ? modelData.subtitle : "")
            image: row.landscape
                   ? (modelData.imageLandscape !== undefined ? modelData.imageLandscape : "")
                   : (modelData.image !== undefined ? modelData.image : "")
            library: row.showLibrary
                     ? (modelData.library !== undefined ? modelData.library : "")
                     : ""
            progress: modelData.progress !== undefined ? modelData.progress : 0
            played: modelData.played !== undefined ? modelData.played : false
            active: row.active && index === row.currentIndex
        }
    }

    function ensureVisible() {
        if (row.items.length === 0) return
        list.positionViewAtIndex(row.currentIndex, ListView.Contain)
    }

    onCurrentIndexChanged: ensureVisible()
    onActiveChanged: if (row.active) ensureVisible()
    onItemsChanged: ensureVisible()
}
