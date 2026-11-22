import QtQuick 2.14
import "."

// Player OSD, track/quality menus, statistics, next episode and
// lost connection screen.
Item {
    id: playerLayer

    property int screenWidth: 1280
    property int screenHeight: 720
    property var labels: ({})

    function formatTime(seconds) {
        var t = Math.floor(seconds > 0 ? seconds : 0)
        var h = Math.floor(t / 3600)
        var m = Math.floor((t % 3600) / 60)
        var sec = t % 60
        var mm = (m < 10 ? "0" : "") + m
        var ss = (sec < 10 ? "0" : "") + sec
        return h > 0 ? (h + ":" + mm + ":" + ss) : (m + ":" + ss)
    }

    visible: app.playerActive
    width: 1280
    height: 720
    scale: Math.min(screenWidth / width, screenHeight / height)
    transformOrigin: Item.TopLeft
    x: Math.round((screenWidth - width * scale) / 2)
    y: Math.round((screenHeight - height * scale) / 2)

    // Player OSD, modeled on Jellyfin: seek bar with knob,
    // transport buttons and tracks. Fades out after 5 s.
    Item {
        id: osdLayer
        anchors.fill: parent
        opacity: app.osdVisible ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 300; easing.type: Easing.InOutQuad } }

        Rectangle {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: 230
            gradient: Gradient {
                GradientStop { position: 0.0; color: Theme.transparent }
                GradientStop { position: 1.0; color: Theme.overlayDark }
            }
        }

        Text {
            id: osdTitle
            anchors.left: parent.left
            anchors.leftMargin: 48
            anchors.right: osdTime.left
            anchors.rightMargin: 24
            anchors.bottom: osdBar.top
            anchors.bottomMargin: 16
            color: Theme.text
            font.pixelSize: 24
            font.bold: true
            elide: Text.ElideRight
            text: app.playerTitle
        }

        Text {
            id: osdTime
            anchors.right: parent.right
            anchors.rightMargin: 48
            anchors.bottom: osdBar.top
            anchors.bottomMargin: 18
            color: app.seekPending ? Theme.text : Theme.textSoft
            font.pixelSize: 18
            text: formatTime(app.seekPending ? app.seekPreview : app.playerPosition) +
                  "  /  " + formatTime(app.playerDuration)
        }

        // seek bar: white = target, light blue = already watched
        Item {
            id: osdBar
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 48
            anchors.rightMargin: 48
            anchors.bottom: osdButtons.top
            anchors.bottomMargin: 24
            height: 24

            Rectangle {
                anchors.fill: parent
                anchors.margins: -10
                radius: 14
                color: Theme.accentGlow
                visible: app.playerFocus === 0 && app.osdVisible
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width
                height: 7
                radius: 3
                color: Theme.track
            }

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: app.playerDuration > 0
                       ? parent.width * Math.min(1, (app.seekPending ? app.seekPreview : app.playerPosition) / app.playerDuration)
                       : 0
                height: 7
                radius: 3
                color: Theme.accent
            }

            Rectangle {
                id: osdKnob
                width: 8
                height: 24
                radius: 4
                color: app.seekPending ? Theme.text : Theme.accentSoft
                anchors.verticalCenter: parent.verticalCenter
                x: app.playerDuration > 0
                   ? Math.max(0, Math.min(parent.width - width,
                         parent.width * (app.seekPending ? app.seekPreview : app.playerPosition) / app.playerDuration - width / 2))
                   : 0
            }

            // tooltip with the target time
            Rectangle {
                visible: app.seekPending
                width: 120
                height: 34
                radius: 8
                color: Theme.overlay
                border.width: 1
                border.color: Theme.accent
                anchors.bottom: parent.top
                anchors.bottomMargin: 4
                x: Math.max(0, Math.min(parent.width - width,
                      osdKnob.x + osdKnob.width / 2 - width / 2))

                Text {
                    anchors.centerIn: parent
                    color: Theme.text
                    font.pixelSize: 16
                    font.bold: true
                    text: formatTime(app.seekPreview)
                }
            }

            // focus on the bar
            Rectangle {
                anchors.fill: parent
                anchors.margins: -8
                radius: 14
                color: "transparent"
                border.width: app.playerFocus === 0 && app.osdVisible ? 2 : 0
                border.color: Theme.accent
            }
        }

        Row {
            id: osdButtons
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 34
            spacing: 10

            PlayerButton {
                text: "-10 s"
                active: app.playerFocus === 1
            }
            PlayerIconButton {
                paused: app.playerPaused
                active: app.playerFocus === 2
            }
            PlayerButton {
                text: "+30 s"
                active: app.playerFocus === 3
            }
            Item { width: 18; height: 1 }
            PlayerButton { text: labels.audio; active: app.playerFocus === 4 }
            PlayerButton { text: labels.subtitles; active: app.playerFocus === 5 }
            PlayerButton { text: labels.quality; active: app.playerFocus === 6 }
            PlayerButton { text: labels.audioDelay; active: app.playerFocus === 7 }
            PlayerButton { text: labels.stats; active: app.playerFocus === 8 }
        }

    }

    // ---------------------------------------- AUDIO/SUBTITLES/QUALITY MENU
    Rectangle {
        id: playerMenu
        visible: app.playerMenuOpen && app.playerMenuMode !== "avdelay"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 96
        width: 520
        height: Math.min(430, 52 + app.playerMenuItems.length * 42)
        radius: 14
        color: Theme.overlayPanel
        border.width: 2
        border.color: Theme.accent
        z: 12

        Text {
            id: playerMenuTitle
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.top: parent.top
            anchors.topMargin: 12
            color: Theme.accentSoft
            font.pixelSize: 16
            font.bold: true
            text: app.playerMenuMode === "audio" ? labels.audio :
                  (app.playerMenuMode === "subtitles" ? labels.subtitles : labels.quality)
        }

        ListView {
            id: playerMenuList
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: playerMenuTitle.bottom
            anchors.topMargin: 8
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 10
            anchors.leftMargin: 10
            anchors.rightMargin: 10
            clip: true
            spacing: 2
            model: app.playerMenuItems
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                width: playerMenuList.width
                height: 40
                radius: 8
                color: index === app.playerMenuIndex ? Theme.selected : "transparent"
                border.width: index === app.playerMenuIndex ? 2 : 0
                border.color: Theme.accent

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: 28
                    anchors.verticalCenter: parent.verticalCenter
                    color: index === app.playerMenuIndex ? Theme.text : Theme.textSoft
                    font.pixelSize: 16
                    font.bold: index === app.playerMenuIndex
                    elide: Text.ElideRight
                    text: modelData.label
                }

                Rectangle {
                    visible: modelData.selected
                    width: 8
                    height: 8
                    radius: 4
                    color: Theme.accentSoft
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        Connections {
            target: app
            onPlayerMenuChanged: {
                if (app.playerMenuOpen)
                    playerMenuList.positionViewAtIndex(app.playerMenuIndex,
                                                     ListView.Contain)
            }
        }
    }

    // ------------------------------------------------ AUDIO DELAY
    Rectangle {
        id: playerDelayPopup
        visible: app.playerMenuOpen && app.playerMenuMode === "avdelay"
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 96
        width: 420
        height: 128
        radius: 14
        color: Theme.overlayPanel
        border.width: 2
        border.color: Theme.accent
        z: 12

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 12
            color: Theme.accentSoft
            font.pixelSize: 16
            font.bold: true
            text: labels.audioDelay
        }

        Row {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 48
            spacing: 24

            Text {
                color: Theme.accentSoft
                font.pixelSize: 30
                font.bold: true
                text: "<"
            }

            Text {
                width: 150
                horizontalAlignment: Text.AlignHCenter
                color: Theme.text
                font.pixelSize: 26
                font.bold: true
                text: (app.audioDelayMs > 0 ? "+" : "") + app.audioDelayMs + " ms"
            }

            Text {
                color: Theme.accentSoft
                font.pixelSize: 30
                font.bold: true
                text: ">"
            }
        }
    }

    // ---------------------------------------------------- CONNECTION LOST
    Rectangle {
        id: stalledCard
        visible: app.playerStalled && app.playerActive
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 56
        width: 620
        height: 142
        radius: 16
        color: Theme.overlayPanel
        border.width: 2
        border.color: Theme.danger
        z: 25

        Row {
            anchors.centerIn: parent
            spacing: 18

            Item {
                width: 42
                height: 42

                Canvas {
                    anchors.fill: parent
                    onPaint: {
                        var ctx = getContext("2d")
                        ctx.reset()
                        ctx.strokeStyle = "#ff8a8a"
                        ctx.lineWidth = 4
                        ctx.lineCap = "round"
                        ctx.beginPath()
                        ctx.arc(21, 21, 15, 0.6, 5.2)
                        ctx.stroke()
                    }
                    RotationAnimation on rotation {
                        from: 0; to: 360; duration: 1000
                        loops: Animation.Infinite
                        running: stalledCard.visible
                    }
                }
            }

            Column {
                spacing: 8

                Text {
                    color: Theme.dangerSoft
                    font.pixelSize: 23
                    font.bold: true
                    text: app.playerStatus
                }

                Rectangle {
                    width: 150
                    height: 34
                    radius: 17
                    color: Theme.dangerBg
                    border.width: 1
                    border.color: Theme.danger

                    Text {
                        anchors.centerIn: parent
                        color: Theme.dangerText
                        font.pixelSize: 15
                        font.bold: true
                        text: labels.resume
                    }
                }
            }
        }
    }

    // ---------------------------------------------------- NEXT EPISODE
    Rectangle {
        id: nextCard
        visible: app.nextEpisodeVisible && !app.playerPaused && !app.osdVisible &&
                 !app.statsOpen
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: 36
        anchors.bottomMargin: 36
        width: 620
        height: 178
        radius: 14
        color: Theme.overlayPanel
        border.width: 2
        border.color: Theme.accent
        z: 8

        Row {
            anchors.fill: parent
            anchors.margins: 12
            spacing: 16

            RoundedImage {
                width: 252
                height: 154
                radius: 10
                source: (app.nextEpisode.imageLandscape !== undefined &&
                         app.nextEpisode.imageLandscape.length > 0)
                        ? app.nextEpisode.imageLandscape
                        : (app.nextEpisode.image !== undefined ? app.nextEpisode.image : "")
                sourceSizeWidth: 520
            }

            Column {
                width: parent.width - 268
                anchors.verticalCenter: parent.verticalCenter
                spacing: 6

                Text {
                    width: parent.width
                    color: Theme.accentSoft
                    font.pixelSize: 14
                    font.bold: true
                    text: labels.nextEpisode
                }
                Text {
                    width: parent.width
                    color: Theme.text
                    font.pixelSize: 23
                    font.bold: true
                    elide: Text.ElideRight
                    text: app.nextEpisode.name !== undefined ? app.nextEpisode.name : ""
                }
                Text {
                    width: parent.width
                    color: Theme.textMuted
                    font.pixelSize: 14
                    elide: Text.ElideRight
                    text: app.nextEpisode.subtitle !== undefined ? app.nextEpisode.subtitle : ""
                }

                Item { width: 1; height: 6 }

                Row {
                    spacing: 8

                    Canvas {
                        width: 20
                        height: 20
                        anchors.verticalCenter: parent.verticalCenter
                        onPaint: {
                            var ctx = getContext("2d")
                            ctx.reset()
                            ctx.fillStyle = "#7fd9ff"
                            ctx.beginPath()
                            ctx.moveTo(5, 3)
                            ctx.lineTo(17, 10)
                            ctx.lineTo(5, 17)
                            ctx.closePath()
                            ctx.fill()
                        }
                    }

                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.accentSoft
                        font.pixelSize: 16
                        font.bold: true
                        text: labels.play
                    }
                }
            }
        }
    }

    // pause symbol: two bars, no font glyph
    Row {
        anchors.centerIn: parent
        spacing: 14
        visible: app.playerPaused

        Rectangle { width: 16; height: 52; radius: 3; color: Theme.text }
        Rectangle { width: 16; height: 52; radius: 3; color: Theme.text }
    }

    // transcoding statistics (Start/Menu key)
    Rectangle {
        visible: app.statsOpen
        z: 6
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 24
        width: 470
        height: statsColumn.height + 28
        radius: 10
        color: Theme.overlayLight
        border.width: 1
        border.color: Theme.borderAccent

        Column {
            id: statsColumn
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 14
            spacing: 4

            Text {
                color: Theme.accentSoft
                font.pixelSize: 16
                font.bold: true
                text: labels.transcoding
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.resolution.arg(app.playerStats.resolution !== undefined ? app.playerStats.resolution : "?")
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: app.playerStats.bandwidth !== undefined
                      ? labels.bitrate.arg((app.playerStats.bandwidth / 1000000).toFixed(1) + " Mbps")
                      : labels.bitrate.arg("?")
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.codec.arg(app.playerStats.codecs !== undefined ? app.playerStats.codecs : "?")
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.fps.arg(app.playerStats.fps !== undefined ? app.playerStats.fps : 0)
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.queueVideo.arg(app.playerStats.queueVideo !== undefined ? app.playerStats.queueVideo : 0)
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.queueAudio.arg(app.playerStats.queueAudio !== undefined
                      ? Math.round(app.playerStats.queueAudio / 48) : 0)
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.segment.arg(app.playerStats.segmentMs !== undefined ? app.playerStats.segmentMs : 0)
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.failed.arg(app.playerStats.failed !== undefined ? app.playerStats.failed : 0)
            }
            Text {
                color: Theme.textSoft
                font.pixelSize: 14
                text: labels.dropped.arg(app.playerStats.dropped !== undefined ? app.playerStats.dropped : 0)
            }
        }
    }

    Text {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 40
        visible: app.playerStatus.length > 0 && !app.playerStalled
        color: Theme.danger
        font.pixelSize: 20
        text: app.playerStatus
    }
}
