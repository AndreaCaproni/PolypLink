import QtQuick 2.14
import "."

// Toast comune a tutta l'app.
Rectangle {
    id: toast

    property string text: ""
    property int bottomOffset: 44

    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottom: parent.bottom
    anchors.bottomMargin: bottomOffset
    width: label.implicitWidth + 40
    height: 38
    radius: 19
    color: Theme.toastBg
    border.width: 1
    border.color: Theme.accent
    opacity: text.length > 0 ? 1 : 0
    visible: opacity > 0
    z: 30

    Behavior on opacity { NumberAnimation { duration: 250 } }

    Text {
        id: label
        anchors.centerIn: parent
        color: Theme.toastText
        font.pixelSize: 15
        text: toast.text
    }
}
