import QtQuick 2.14

// Pulsante azione compatto: solo icona quando non ha il focus, icona + testo
// quando lo ha. Icone disegnate (niente font di simboli).
Rectangle {
    id: button

    property string label: ""
    // play | replay | check | heart-empty | heart-filled | horse | series | previous
    property string icon: "play"
    property bool active: false
    property bool primary: false
    property color customIconColor: "transparent"

    width: active ? 56 + 10 + labelText.implicitWidth + 18 : 56
    height: 48
    radius: 24

    color: primary ? (active ? "#1d5a75" : "#00a4dc")
                   : (active ? "#123243" : "#101920")
    border.width: active ? 2 : 1
    border.color: active ? "#7fd9ff" : (primary ? "#7fd9ff" : "#1b2733")

    Behavior on width { NumberAnimation { duration: 160; easing.type: Easing.OutCubic } }

    Canvas {
        id: iconCanvas
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        visible: button.icon !== "horse"

        property color tint: button.customIconColor.a > 0
                              ? button.customIconColor
                              : (button.primary && !button.active ? "#00121a" : "#ffffff")
        property string iconName: button.icon
        onTintChanged: requestPaint()
        onIconNameChanged: requestPaint()

        onPaint: {
            var ctx = getContext("2d")
            ctx.reset()
            ctx.strokeStyle = tint
            ctx.fillStyle = tint
            ctx.lineWidth = 2.4
            ctx.lineCap = "round"
            ctx.lineJoin = "round"
            if (button.icon === "play") {
                ctx.beginPath()
                ctx.moveTo(8, 5)
                ctx.lineTo(20, 12)
                ctx.lineTo(8, 19)
                ctx.closePath()
                ctx.fill()
            } else if (button.icon === "replay") {
                ctx.beginPath()
                ctx.arc(12, 12, 7, 0.7, 5.2)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(3.5, 7)
                ctx.lineTo(8.5, 6.5)
                ctx.lineTo(7, 11)
                ctx.closePath()
                ctx.fill()
            } else if (button.icon === "check") {
                ctx.beginPath()
                ctx.moveTo(4.5, 13)
                ctx.lineTo(9.5, 18)
                ctx.lineTo(20, 6)
                ctx.stroke()
            } else if (button.icon === "text") {
                ctx.beginPath(); ctx.moveTo(4, 6); ctx.lineTo(20, 6); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(4, 12); ctx.lineTo(20, 12); ctx.stroke()
                ctx.beginPath(); ctx.moveTo(4, 18); ctx.lineTo(15, 18); ctx.stroke()
            } else if (button.icon === "heart-empty" || button.icon === "heart-filled") {
                ctx.beginPath()
                ctx.moveTo(12, 20)
                ctx.bezierCurveTo(2, 13, 3.5, 4.5, 12, 9)
                ctx.bezierCurveTo(20.5, 4.5, 22, 13, 12, 20)
                ctx.closePath()
                if (button.icon === "heart-filled") ctx.fill()
                else ctx.stroke()
            } else if (button.icon === "check-double") {
                ctx.beginPath()
                ctx.moveTo(3, 12.5)
                ctx.lineTo(7.5, 17)
                ctx.lineTo(16, 5.5)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(9, 12.5)
                ctx.lineTo(13.5, 17)
                ctx.lineTo(22, 5.5)
                ctx.stroke()
            } else if (button.icon === "series") {
                ctx.beginPath()
                ctx.rect(4.5, 8, 15, 10)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(6.5, 6)
                ctx.lineTo(19, 6)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(8.5, 4)
                ctx.lineTo(16.5, 4)
                ctx.stroke()
            } else if (button.icon === "previous") {
                ctx.fillRect(4.5, 6, 3, 12)
                ctx.beginPath()
                ctx.moveTo(20, 6)
                ctx.lineTo(10, 12)
                ctx.lineTo(20, 18)
                ctx.closePath()
                ctx.fill()
            }
        }
    }

    Image {
        visible: button.icon === "horse"
        source: "qrc:/qml/assets/horse.png"
        x: 16
        anchors.verticalCenter: parent.verticalCenter
        width: 24
        height: 24
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    Text {
        id: labelText
        x: 46
        anchors.verticalCenter: parent.verticalCenter
        opacity: button.active ? 1 : 0
        visible: opacity > 0
        color: button.primary && !button.active ? "#00121a" : "#ffffff"
        font.pixelSize: 15
        text: button.label

        Behavior on opacity { NumberAnimation { duration: 140 } }
    }
}
