import QtQuick 2.14

// Play/pausa con simboli disegnati (niente glifi da font): due barre per la
// pausa, triangolo per il play. Stessa pillola degli altri pulsanti OSD.
Rectangle {
    id: button

    property bool paused: false
    property bool active: false

    width: 92
    height: 44
    radius: 22
    color: active ? "#1d5a75" : "#00a4dc"
    border.width: active ? 2 : 1
    border.color: "#7fd9ff"

    Row {
        anchors.centerIn: parent
        spacing: 7
        visible: !button.paused

        Rectangle { width: 7; height: 22; radius: 2; color: button.active ? "#ffffff" : "#00121a" }
        Rectangle { width: 7; height: 22; radius: 2; color: button.active ? "#ffffff" : "#00121a" }
    }

    Canvas {
        id: playIcon
        visible: button.paused
        width: 26
        height: 26
        anchors.centerIn: parent

        onVisibleChanged: requestPaint()
        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.fillStyle = button.active ? "#ffffff" : "#00121a"
            ctx.beginPath()
            ctx.moveTo(7, 4)
            ctx.lineTo(22, 13)
            ctx.lineTo(7, 22)
            ctx.closePath()
            ctx.fill()
        }
    }
}
