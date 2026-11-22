import QtQuick 2.14
import "."

// Home: Continue watching, Next up, Recently added and libraries rows.
Item {
    id: homePage

    property var resumeItems: []
    property var nextUpItems: []
    property var latestItems: []
    property var libraryViews: []
    property int rowIndex: 0
    property int colIndex: 0
    property bool navFocus: false
    property string resumeTitle: ""
    property string nextUpTitle: ""
    property string latestTitle: ""
    property string librariesTitle: ""

    Flickable {
        id: flick
        anchors.fill: parent
        contentWidth: width
        contentHeight: homeColumn.height + 40
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: homeColumn
            width: parent.width
            spacing: 18
            topPadding: 12

            MediaRow {
                id: rowResume
                width: parent.width
                title: homePage.resumeTitle
                items: homePage.resumeItems
                landscape: true
                active: homePage.rowIndex === 0 && !homePage.navFocus
                currentIndex: homePage.colIndex
                visible: homePage.resumeItems.length > 0
            }

            MediaRow {
                id: rowNextUp
                width: parent.width
                title: homePage.nextUpTitle
                items: homePage.nextUpItems
                landscape: true
                active: homePage.rowIndex === 1 && !homePage.navFocus
                currentIndex: homePage.colIndex
                visible: homePage.nextUpItems.length > 0
            }

            MediaRow {
                id: rowLatest
                width: parent.width
                title: homePage.latestTitle
                items: homePage.latestItems
                showLibrary: true
                active: homePage.rowIndex === 2 && !homePage.navFocus
                currentIndex: homePage.colIndex
                visible: homePage.latestItems.length > 0
            }

            Item {
                id: rowLibs
                width: parent.width
                height: 150
                visible: homePage.libraryViews.length > 0

                Text {
                    id: libTitle
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.top: parent.top
                    color: homePage.rowIndex === 3 ? Theme.text : Theme.textMuted
                    font.pixelSize: 24
                    font.bold: true
                    text: homePage.librariesTitle
                }

                ListView {
                    id: libList
                    anchors.top: libTitle.bottom
                    anchors.topMargin: 10
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.leftMargin: 24
                    anchors.rightMargin: 24
                    height: 96
                    orientation: ListView.Horizontal
                    model: homePage.libraryViews
                    spacing: 14
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: 190
                        height: 90
                        radius: 10
                        color: homePage.rowIndex === 3 && index === homePage.colIndex &&
                               !homePage.navFocus ? Theme.selected : Theme.panel
                        border.width: homePage.rowIndex === 3 && index === homePage.colIndex &&
                                      !homePage.navFocus ? 2 : 1
                        border.color: homePage.rowIndex === 3 && index === homePage.colIndex &&
                                      !homePage.navFocus ? Theme.accent : Theme.border

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            color: Theme.text
                            font.pixelSize: 19
                            elide: Text.ElideRight
                            text: modelData.name
                        }
                    }
                }

                function ensureVisible() {
                    if (homePage.libraryViews.length === 0) return
                    libList.positionViewAtIndex(homePage.colIndex, ListView.Contain)
                }
            }
        }
    }

    function ensureRowVisible(row) {
        var y = 0
        if (row === 0) { rowResume.ensureVisible(); y = rowResume.y }
        else if (row === 1) { rowNextUp.ensureVisible(); y = rowNextUp.y }
        else if (row === 2) { rowLatest.ensureVisible(); y = rowLatest.y }
        else { rowLibs.ensureVisible(); y = rowLibs.y }

        var maxY = Math.max(0, homeColumn.height - flick.height)
        flick.contentY = Math.max(0, Math.min(maxY, y - 70))
    }
}
