import QtQuick 2.14
import "."

// Barra superiore: logo, schede Home/Preferiti, utente e impostazioni.
Rectangle {
    id: header

    property bool navFocus: false
    property int navIndex: 0
    property string page: "home"
    property string appState: ""
    property string serverLabel: ""
    property string userName: ""

    width: parent.width
    height: 56
    color: Theme.header

    // logo PolypLink + nome dell'app
    Row {
        x: 24
        anchors.verticalCenter: parent.verticalCenter
        spacing: 10

        Image {
            width: 48
            height: 48
            anchors.verticalCenter: parent.verticalCenter
            source: "qrc:/qml/assets/polyplink-logo.png"
            fillMode: Image.PreserveAspectFit
            smooth: true
        }

        Row {
            spacing: 0
            anchors.verticalCenter: parent.verticalCenter
            Text {
                color: Theme.text
                font.pixelSize: 26
                font.bold: true
                text: "Polyp"
            }
            Text {
                color: Theme.accent
                font.pixelSize: 26
                font.bold: true
                text: "Link"
            }
        }

        Rectangle {
            width: 46
            height: 20
            radius: 10
            color: Theme.chip
            border.width: 1
            border.color: Theme.borderAccent

            Text {
                anchors.centerIn: parent
                color: Theme.accentSoft
                font.pixelSize: 11
                font.bold: true
                text: "beta"
            }
        }
    }

    // schede
    Rectangle {
        id: homeTab
        x: 220
        anchors.verticalCenter: parent.verticalCenter
        width: 96
        height: 36
        radius: 18
        visible: appState === "home"
        color: navFocus && navIndex === 0 ? Theme.selected : "transparent"
        border.width: navFocus && navIndex === 0 ? 2 : 0
        border.color: Theme.accent
        Text {
            anchors.centerIn: parent
            text: qsTr("Home")
            font.pixelSize: 17
            font.bold: page === "home"
            color: page === "home" ? Theme.text : Theme.textMuted
        }
    }

    Rectangle {
        id: favTab
        x: 324
        anchors.verticalCenter: parent.verticalCenter
        width: 128
        height: 36
        radius: 18
        visible: appState === "home"
        color: navFocus && navIndex === 1 ? Theme.selected : "transparent"
        border.width: navFocus && navIndex === 1 ? 2 : 0
        border.color: Theme.accent
        Text {
            anchors.centerIn: parent
            text: qsTr("Preferiti")
            font.pixelSize: 17
            font.bold: page === "favorites"
            color: page === "favorites" ? Theme.text : Theme.textMuted
        }
    }

    Text {
        anchors.left: parent.left
        anchors.leftMargin: 252
        anchors.verticalCenter: parent.verticalCenter
        color: Theme.textDim
        font.pixelSize: 17
        visible: appState !== "home"
        text: appState === "login" ? qsTr("scegli il server") : serverLabel
    }

    // account: avatar + nome utente, pillola che cresce verso sinistra
    Rectangle {
        id: accountBtn
        height: 40
        radius: 20
        width: avatarRow.implicitWidth + 28
        visible: appState === "home"
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: settingsBtn.left
        anchors.rightMargin: 10
        color: navFocus && navIndex === 2 ? Theme.selected : Theme.chip
        border.width: navFocus && navIndex === 2 ? 2 : 1
        border.color: navFocus && navIndex === 2 ? Theme.accent : Theme.borderStrong

        Row {
            id: avatarRow
            anchors.centerIn: parent
            spacing: 8

            Item {
                width: 24
                height: 24
                anchors.verticalCenter: parent.verticalCenter

                Rectangle {
                    width: 10; height: 10; radius: 5
                    color: Theme.textSoft
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 2
                }
                Rectangle {
                    width: 18; height: 9; radius: 4.5
                    color: Theme.textSoft
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: 14
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: Math.min(implicitWidth, 160)
                elide: Text.ElideRight
                color: Theme.text
                font.pixelSize: 16
                text: userName
            }
        }
    }

    // ingranaggio impostazioni
    Rectangle {
        id: settingsBtn
        width: 40
        height: 40
        radius: 20
        visible: appState === "home"
        anchors.verticalCenter: parent.verticalCenter
        anchors.right: parent.right
        anchors.rightMargin: 24
        color: navFocus && navIndex === 3 ? Theme.selected : Theme.chip
        border.width: navFocus && navIndex === 3 ? 2 : 1
        border.color: navFocus && navIndex === 3 ? Theme.accent : Theme.borderStrong

        Canvas {
            id: gearIcon
            anchors.centerIn: parent
            width: 24
            height: 24
            property color tint: navFocus && navIndex === 3 ? Theme.text : Theme.textSoft
            onTintChanged: requestPaint()
            onPaint: {
                var ctx = getContext("2d")
                ctx.reset()
                ctx.translate(width / 2, height / 2)
                ctx.fillStyle = tint
                for (var i = 0; i < 8; ++i) {
                    ctx.save()
                    ctx.rotate(i * Math.PI / 4)
                    ctx.fillRect(-2, -11, 4, 4)
                    ctx.restore()
                }
                ctx.beginPath()
                ctx.arc(0, 0, 7.5, 0, 2 * Math.PI)
                ctx.fill()
                ctx.globalCompositeOperation = "destination-out"
                ctx.beginPath()
                ctx.arc(0, 0, 3.2, 0, 2 * Math.PI)
                ctx.fill()
            }
        }
    }
}
