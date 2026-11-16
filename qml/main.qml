import QtQuick 2.14
import QtQuick.Window 2.14

Window {
    id: root
    // La finestra resta visibile: nascondendola, su questo dispositivo EGLFS
    // spegne il display e il piano video non viene piu' composto.
    visible: true
    width: Screen.width > 0 ? Screen.width : 1280
    height: Screen.height > 0 ? Screen.height : 720
    color: app.playerActive ? "#00000000" : "#0b0f14"

    // --- navigazione ---------------------------------------------------------
    property string page: "home"
    property bool detailOpen: false
    property bool overviewOpen: false
    property bool languageOpen: false
    property int languageIndex: 0
    // Dopo "segna come visto" il pulsante replay puo' sparire: il focus deve
    // seguire il pulsante Visto, non restare sull'indice numerico.
    property bool pendingPlayedFocus: false
    property int rowIndex: 0
    // barra superiore: si raggiunge con "su" dalla prima riga
    property bool navFocus: false
    property int navIndex: 0
    // pagina stagione: focus sui pulsanti (true) o sugli episodi (false)
    property bool seasonActionsFocus: false
    property int seasonAction: 0

    // menu laterali di Impostazioni e Account: voci previste per la versione
    // stabile, per ora segnaposto.
    property int menuIndex: 0
    property string menuHint: ""

    property var settingsMenu: [
        { title: qsTr("Server e connessione"), desc: qsTr("Indirizzo, porta, HTTPS e riconnessione automatica"), action: "test" },
        { title: qsTr("Qualità massima"), desc: qsTr("Bitrate predefinito: 4, 8, 12, 20 Mbps o automatico"), action: "quality" },
        { title: qsTr("Risoluzione massima"), desc: qsTr("720p, 1080p o automatica") },
        { title: qsTr("Audio preferito"), desc: qsTr("Lingua e canali (stereo / 5.1)") },
        { title: qsTr("Sottotitoli"), desc: qsTr("Lingua predefinita, attivazione e burn-in") },
        { title: qsTr("Precaricamento"), desc: qsTr("Quanti segmenti tenere pronti prima di partire") },
        { title: qsTr("Seek e accelerazione"), desc: qsTr("Passo iniziale e tetto dell'accelerazione") },
        { title: qsTr("OSD"), desc: qsTr("Durata, contenuti e comportamento in pausa"), action: "osd" },
        { title: qsTr("Lingua dell'interfaccia"), desc: qsTr("Lingua dell'interfaccia; titoli e trame seguono il server"), action: "language" },
        { title: qsTr("Scala dell'interfaccia"), desc: qsTr("Dimensione di testi e card") },
        { title: qsTr("Tema"), desc: qsTr("Scuro, chiaro, automatico") },
        { title: qsTr("Rete e certificati"), desc: qsTr("Proxy e certificati HTTPS non firmati") },
        { title: qsTr("Cache immagini"), desc: qsTr("Dimensione massima e pulizia") },
        { title: qsTr("Diagnostica e log"), desc: qsTr("Livello dei log ed esportazione su USB") },
        { title: qsTr("Aggiornamenti"), desc: qsTr("Canale beta e controllo manuale") },
        { title: qsTr("Ripristino"), desc: qsTr("Riporta le impostazioni ai valori predefiniti"), action: "reset" },
        { title: qsTr("Informazioni"), desc: qsTr("Versione, build, licenze e componenti"), action: "info" }
    ]

    property var accountMenu: [
        { title: qsTr("Profilo"), desc: qsTr("Nome utente, avatar e server collegato"), action: "profile" },
        { title: qsTr("Cambia utente"), desc: qsTr("Torna alla schermata di accesso"), action: "logout" },
        { title: qsTr("Esci"), desc: qsTr("Chiude la sessione e cancella il token"), action: "logout" },
        { title: qsTr("Cambia server"), desc: qsTr("Scegli un altro Jellyfin"), action: "switchserver" },
        { title: qsTr("Quick Connect"), desc: qsTr("Associa l'app senza digitare la password") },
        { title: qsTr("Sessione e server"), desc: qsTr("Stato della connessione e versione del server"), action: "info" },
        { title: qsTr("Ripresa"), desc: qsTr("Sincronizza la posizione di visione") },
        { title: qsTr("Righe della home"), desc: qsTr("Scegli quali righe mostrare") },
        { title: qsTr("Prossimo episodio"), desc: qsTr("Avvio automatico a fine puntata") },
        { title: qsTr("Statistiche di visione"), desc: qsTr("Tempo visto e cronologia") }
    ]

    Timer {
        id: playedFocusTimer
        interval: 2000
        onTriggered: root.pendingPlayedFocus = false
    }

    Timer {
        id: menuHintTimer
        interval: 1800
        onTriggered: root.menuHint = ""
    }
    property int colIndex: 0
    property int gridIndex: 0
    property int serverIndex: 0
    property int seasonIndex: 0
    property int detailFocus: 0
    property int epIndex: 0

    // --- accesso -------------------------------------------------------------
    property string authStep: "menu"     // menu | credentials
    property int authIndex: 0
    property string credUser: ""
    property string credPass: ""

    // --- tastiera ------------------------------------------------------------
    property bool keyboardOpen: false
    property string kbTarget: ""

    Connections {
        target: app
        onStateChanged: {
            if (app.state === "auth") {
                root.authStep = "menu"
                root.authIndex = 0
                root.credPass = ""
            }
            if (app.state === "home") {
                root.page = "home"
                root.detailOpen = false
            }
        }
    }

    function clamp(v, lo, hi) { return Math.max(lo, Math.min(hi, v)) }

    function fmtTime(seconds) {
        var t = Math.floor(seconds > 0 ? seconds : 0)
        var h = Math.floor(t / 3600)
        var m = Math.floor((t % 3600) / 60)
        var sec = t % 60
        var mm = (m < 10 ? "0" : "") + m
        var ss = (sec < 10 ? "0" : "") + sec
        return h > 0 ? (h + ":" + mm + ":" + ss) : (m + ":" + ss)
    }

    function trackLabel(list, idx) {
        for (var i = 0; i < list.length; ++i)
            if (list[i].index === idx) return list[i].label
        return "—"
    }

    function audioLabel() {
        if (app.audioTracks.length === 0) return "—"
        return root.trackLabel(app.audioTracks, app.audioIndex)
    }

    function subtitleLabel() {
        if (app.subtitleIndex < 0) return qsTr("Spenti")
        return root.trackLabel(app.subtitleTracks, app.subtitleIndex)
    }

    function countItems(n) {
        return n === 1 ? qsTr("1 elemento") : qsTr("%1 elementi").arg(n)
    }

    function countEpisodes(n) {
        return n === 1 ? qsTr("1 episodio") : qsTr("%1 episodi").arg(n)
    }

    function countSeasons(n) {
        return n === 1 ? qsTr("1 stagione") : qsTr("%1 stagioni").arg(n)
    }

    function metaLine(d) {
        var parts = []
        if (d.year !== undefined && d.year > 0) parts.push("" + d.year)
        if (d.duration !== undefined && d.duration.length > 0) parts.push(d.duration)
        if (d.resolution !== undefined && d.resolution.length > 0) parts.push(d.resolution)
        if (d.audio !== undefined && d.audio.length > 0) parts.push(d.audio)
        if (d.rating !== undefined && d.rating > 0) parts.push("★ " + d.rating.toFixed(1))
        if (d.officialRating !== undefined && d.officialRating.length > 0) parts.push(d.officialRating)
        return parts.join("   ·   ")
    }

    function maskText(s) {
        var o = ""
        for (var i = 0; i < s.length; ++i) o += "•"
        return o
    }

    function rowLength(i) {
        if (i === 0) return app.resume.length
        if (i === 1) return app.nextUp.length
        if (i === 2) return app.latest.length
        return app.views.length
    }

    function ensureRowVisible() {
        var y = 0
        if (rowIndex === 0) { rowResume.ensureVisible(); y = rowResume.y }
        else if (rowIndex === 1) { rowNextUp.ensureVisible(); y = rowNextUp.y }
        else if (rowIndex === 2) { rowLatest.ensureVisible(); y = rowLatest.y }
        else { rowLibs.ensureVisible(); y = rowLibs.y }

        var maxY = Math.max(0, homeColumn.height - homeFlick.height)
        homeFlick.contentY = Math.max(0, Math.min(maxY, y - 70))
    }

    function openDetail(id) {
        root.seasonIndex = 0
        root.detailFocus = 0
        root.overviewOpen = false
        app.openItem(id)
        root.detailOpen = true
    }

    // Come nel client ufficiale: se c'e' una ripresa, accanto a "Riprendi"
    // compare "Guarda dall'inizio". Gli indici di focus si spostano di uno.
    function hasResume() {
        return app.detail.resumeTicks !== undefined && app.detail.resumeTicks > 0
    }

    function isSeries() {
        return app.detail.type === "Series"
    }

    // Episodio: ultimo pulsante "Vai alla serie completa".
    function hasSeriesLink() {
        return app.detail.type === "Episode" &&
               app.detail.seriesId !== undefined &&
               app.detail.seriesId.length > 0
    }

    function seriesIndex() {
        return hasSeriesLink() ? (hasResume() ? 4 : 3) : -1
    }

    function isEpisode() {
        return app.detail.type === "Episode"
    }

    function previousIndex() {
        if (!isEpisode()) return -1
        return (hasResume() ? 4 : 3) + (hasSeriesLink() ? 1 : 0)
    }

    // Per le serie: 0 Riproduci, 1 Stagioni, 2 Visto, 3 Preferito (niente
    // impostazioni di riproduzione). Per film/episodi restano gli indici di
    // prima, con lo slot extra per la serie quando serve.
    function hasOverview() {
        return app.detail.overview !== undefined && app.detail.overview.length > 0
    }

    function actionBase() {
        if (isSeries()) return 4
        var n = hasResume() ? 4 : 3
        if (hasSeriesLink()) n += 1
        if (isEpisode()) n += 1
        return n
    }

    function readMoreIndex() {
        return hasOverview() ? actionBase() : -1
    }

    function settingsStart() {
        if (isSeries()) return -1
        return actionBase() + (hasOverview() ? 1 : 0)
    }

    function isProfileView() {
        return page === "account" && currentMenuEntry().action === "profile"
    }

    function playedIndex() {
        return isSeries() ? 2 : (hasResume() ? 2 : 1)
    }

    function favoriteIndex() {
        return isSeries() ? 3 : (hasResume() ? 3 : 2)
    }

    function detailAction() {
        var s = settingsStart()
        if (root.detailFocus === 0) {
            if (isSeries()) app.playSeries(app.detail.id)
            else app.playItem(app.detail.id)
        }
        else if (isSeries() && root.detailFocus === 1) openSeasonsPage()
        else if (!isSeries() && hasResume() && root.detailFocus === 1) app.playFromStart()
        else if (root.detailFocus === playedIndex()) {
            root.pendingPlayedFocus = true
            playedFocusTimer.restart()
            app.togglePlayed()
        }
        else if (root.detailFocus === favoriteIndex()) app.toggleFavorite()
        else if (root.detailFocus === seriesIndex()) openDetail(app.detail.seriesId)
        else if (root.detailFocus === previousIndex()) app.openPreviousEpisode()
        else if (root.detailFocus === readMoreIndex()) root.overviewOpen = true
        else if (s >= 0 && root.detailFocus === s) app.cycleQuality(1)
        else if (s >= 0 && root.detailFocus === s + 1) app.cycleAudio(1)
        else if (s >= 0 && root.detailFocus === s + 2) app.cycleSubtitle(1)
    }

    function openSeasonsPage() {
        root.seasonIndex = 0
        root.detailOpen = false
        root.page = "seasons"
    }

    function seasonActionConfirm() {
        if (seasonAction === 0) app.playSeasonFromStart()
        else if (seasonAction === 1) app.toggleSeasonPlayed(app.seasonInfo.id)
        else app.toggleSeasonFavorite(app.seasonInfo.id)
    }

    function openSeasonNow() {
        if (app.seasons.length === 0) return
        root.epIndex = 0
        root.seasonActionsFocus = false
        root.seasonAction = 0
        app.openSeason(root.seasonIndex)
        root.detailOpen = false
        root.page = "episodes"
    }

    function homeEmpty() {
        return app.resume.length + app.nextUp.length + app.latest.length +
               app.views.length === 0
    }

    function menuValue(e) {
        if (e.action === "quality") return (app.qualityDefaultKbps / 1000) + " Mbps"
        if (e.action === "osd") return qsTr("%1 secondi").arg(app.osdSeconds)
        if (e.action === "test")
            return app.serverLabel.length > 0 ? app.serverLabel : qsTr("Nessun server")
        if (e.action === "language")
            return root.currentLanguageName()
        if (e.action === "profile" || e.action === "info") {
            if (page === "account") {
                if (e.action === "profile") return app.userName
                return app.serverLabel
            }
            return "PolypLink " + appVersion
        }
        return ""
    }

    function currentLanguageIndex() {
        for (var i = 0; i < language.available.length; ++i)
            if (language.available[i].code === language.current) return i
        return 0
    }

    function currentLanguageName() {
        for (var i = 0; i < language.available.length; ++i)
            if (language.available[i].code === language.current)
                return language.available[i].name
        return language.current
    }

    function currentMenuEntry() {
        var list = (page === "settings") ? settingsMenu : accountMenu
        if (menuIndex < 0 || menuIndex >= list.length)
            return { title: "", desc: "", action: "" }
        return list[menuIndex]
    }

    function savedServerRow() {
        return app.serverLabel.length > 0 ? 1 : 0
    }

    function firstRow() {
        for (var i = 0; i < 4; ++i) if (rowLength(i) > 0) return i
        return 0
    }

    function focusContent() {
        navFocus = false
        for (var i = 0; i < 4; ++i) {
            if (rowLength(i) > 0) {
                rowIndex = i
                colIndex = Math.min(colIndex, rowLength(i) - 1)
                ensureRowVisible()
                return
            }
        }
    }

    function ensureValidFocus() {
        if (navFocus) return
        if (rowLength(rowIndex) === 0) focusContent()
    }

    Connections {
        target: app
        onDetailChanged: {
            if (!root.pendingPlayedFocus) return
            root.pendingPlayedFocus = false
            root.detailFocus = root.playedIndex()
        }
    }

    Connections {
        target: app
        onHomeChanged: {
            if (app.state === "home" && root.page === "home" && !root.detailOpen)
                root.ensureValidFocus()
        }
        onStateChanged: {
            if (app.state === "home" && root.page === "home") {
                root.focusContent()
            } else if (app.state === "login") {
                // niente pagina "account" appesa dopo un logout
                root.page = "home"
                root.detailOpen = false
                root.navFocus = false
                root.menuHint = ""
                root.serverIndex = 0
            }
        }
    }

    function activateNav() {
        if (navIndex === 0) {
            page = "home"
            detailOpen = false
            focusContent()
        } else if (navIndex === 1) {
            app.openFavorites()
            page = "favorites"
            gridIndex = 0
            navFocus = false
        } else if (navIndex === 2) {
            page = "account"
            navFocus = false
            menuIndex = 0
            menuHint = ""
            app.refreshUserStats()
        } else {
            page = "settings"
            navFocus = false
            menuIndex = 0
            menuHint = ""
        }
    }

    function moveRow(delta) {
        var r = rowIndex
        for (var n = 0; n < 4; ++n) {
            r += delta
            if (r < 0 || r > 3) return
            if (rowLength(r) > 0) break
        }
        rowIndex = r
        colIndex = clamp(colIndex, 0, Math.max(0, rowLength(rowIndex) - 1))
        ensureRowVisible()
    }

    function moveCol(delta) {
        if (page === "library" || page === "favorites") {
            gridIndex = clamp(gridIndex + delta, 0, Math.max(0, app.libraryItems.length - 1))
            libraryGrid.positionViewAtIndex(gridIndex, GridView.Contain)
            return
        }
        var len = rowLength(rowIndex)
        if (len === 0) return
        colIndex = clamp(colIndex + delta, 0, len - 1)
        ensureRowVisible()
    }

    function moveGridRow(delta) {
        // le colonne reali: quelle che il GridView sta disegnando
        var cols = Math.max(1, Math.floor(libraryGrid.width / libraryGrid.cellWidth))
        var count = app.libraryItems.length
        if (count === 0) return

        var next = gridIndex + delta * cols
        if (next < 0) {                            // sopra la prima riga: barra
            navFocus = true
            navIndex = (page === "favorites") ? 1 : 0
            return
        }
        if (next >= count) return                  // non c'e' una riga sotto

        gridIndex = next
        libraryGrid.positionViewAtIndex(gridIndex, GridView.Contain)
    }

    function openKeyboard(target, current) {
        kbTarget = target
        keyboard.text = current
        keyboard.row = 0
        keyboard.col = 0
        keyboardOpen = true
    }

    function activate() {
        if (app.state === "login") {
            var off = savedServerRow()
            if (off === 1 && serverIndex === 0) {
                app.useSavedServer()
                return
            }
            var si = serverIndex - off
            if (si >= app.servers.length) openKeyboard("url", "http://")
            else app.selectServer(si)
            return
        }

        if (app.state === "auth") {
            if (authStep === "menu") {
                if (authIndex === 0) authStep = "credentials"
                else app.beginQuickConnect()
                return
            }
            // credentials
            if (authIndex === 0) openKeyboard("user", credUser)
            else if (authIndex === 1) openKeyboard("pass", credPass)
            else app.loginWithPassword(credUser, credPass)
            return
        }

        if (app.state === "quickconnect") return

        if (detailOpen) {
            detailAction()
            return
        }

        if (page === "library" || page === "favorites") {
            if (app.libraryItems.length === 0) return
            var it = app.libraryItems[gridIndex]
            if (it !== undefined) openDetail(it.id)
            return
        }

        if (rowIndex === 3) {
            if (app.views.length === 0) return
            app.openView(colIndex)
            gridIndex = 0
            page = "library"
            return
        }

        var m = rowIndex === 0 ? app.resume : (rowIndex === 1 ? app.nextUp : app.latest)
        if (!m || m.length === 0) return
        var item = m[clamp(colIndex, 0, m.length - 1)]
        if (item !== undefined) openDetail(item.id)
    }

    function goBack() {
        if (languageOpen) { languageOpen = false; return }
        if (overviewOpen) { overviewOpen = false; return }
        if (keyboardOpen) { keyboardOpen = false; return }
        if (detailOpen) { detailOpen = false; return }
        if (page === "seasons") {
            root.detailOpen = true
            root.page = "home"
            app.reopenSeries()
            return
        }
        if (page === "episodes") {
            page = "seasons"
            detailOpen = false
            return
        }
        if (page === "library" || page === "favorites") { page = "home"; return }
        if (page === "settings" || page === "account") {
            var back = (page === "settings") ? 3 : 2
            page = "home"
            navFocus = true
            navIndex = back
            return
        }
        if (app.state === "auth" && authStep === "credentials") { authStep = "menu"; return }
        if (app.state === "quickconnect") { app.cancelQuickConnect(); return }
        Qt.quit()
    }

    // --- tela virtuale 1280x720, scalata sulla risoluzione reale ------------
    // Tutta la UI e' disegnata in coordinate 1280x720. Qui viene scalata per
    // riempire lo schermo mantenendo le proporzioni: su un 1080p (o altro)
    // nulla si rompe e nulla diventa microscopico.
    Item {
        id: canvas
        visible: !app.playerActive
        width: 1280
        height: 720
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.TopLeft
        x: Math.round((root.width - width * scale) / 2)
        y: Math.round((root.height - height * scale) / 2)

    // ------------------------------------------------------------------ sfondo
    Rectangle {
        anchors.fill: parent
        visible: !app.playerActive
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0b0f14" }
            GradientStop { position: 1.0; color: "#101922" }
        }
    }

    // ------------------------------------------------------------------ header
    Rectangle {
        id: header
        width: parent.width
        height: 56
        color: "#111a23"

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
                    color: "#ffffff"
                    font.pixelSize: 26
                    font.bold: true
                    text: "Polyp"
                }
                Text {
                    color: "#00a4dc"
                    font.pixelSize: 26
                    font.bold: true
                    text: "Link"
                }
            }

            Rectangle {
                width: 46
                height: 20
                radius: 10
                color: "#182430"
                border.width: 1
                border.color: "#2b6f8c"

                Text {
                    anchors.centerIn: parent
                    color: "#7fd9ff"
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
            visible: app.state === "home"
            color: root.navFocus && root.navIndex === 0 ? "#123243" : "transparent"
            border.width: root.navFocus && root.navIndex === 0 ? 2 : 0
            border.color: "#00a4dc"
            Text {
                anchors.centerIn: parent
                text: qsTr("Home")
                font.pixelSize: 17
                font.bold: root.page === "home"
                color: root.page === "home" ? "#ffffff" : "#93a4b5"
            }
        }

        Rectangle {
            id: favTab
            x: 324
            anchors.verticalCenter: parent.verticalCenter
            width: 128
            height: 36
            radius: 18
            visible: app.state === "home"
            color: root.navFocus && root.navIndex === 1 ? "#123243" : "transparent"
            border.width: root.navFocus && root.navIndex === 1 ? 2 : 0
            border.color: "#00a4dc"
            Text {
                anchors.centerIn: parent
                text: qsTr("Preferiti")
                font.pixelSize: 17
                font.bold: root.page === "favorites"
                color: root.page === "favorites" ? "#ffffff" : "#93a4b5"
            }
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 252
            anchors.verticalCenter: parent.verticalCenter
            color: "#6f8090"
            font.pixelSize: 17
            visible: app.state !== "home"
            text: app.state === "login" ? qsTr("scegli il server") : app.serverLabel
        }

        // account: avatar + nome utente, pillola che cresce verso sinistra
        Rectangle {
            id: accountBtn
            height: 40
            radius: 20
            width: avatarRow.implicitWidth + 28
            visible: app.state === "home"
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: settingsBtn.left
            anchors.rightMargin: 10
            color: root.navFocus && root.navIndex === 2 ? "#123243" : "#182430"
            border.width: root.navFocus && root.navIndex === 2 ? 2 : 1
            border.color: root.navFocus && root.navIndex === 2 ? "#00a4dc" : "#243040"

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
                        color: "#c9d4df"
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 2
                    }
                    Rectangle {
                        width: 18; height: 9; radius: 4.5
                        color: "#c9d4df"
                        anchors.horizontalCenter: parent.horizontalCenter
                        y: 14
                    }
                }

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(implicitWidth, 160)
                    elide: Text.ElideRight
                    color: "#e6eef5"
                    font.pixelSize: 16
                    text: app.userName
                }
            }
        }

        // ingranaggio impostazioni
        Rectangle {
            id: settingsBtn
            width: 40
            height: 40
            radius: 20
            visible: app.state === "home"
            anchors.verticalCenter: parent.verticalCenter
            anchors.right: parent.right
            anchors.rightMargin: 24
            color: root.navFocus && root.navIndex === 3 ? "#123243" : "#182430"
            border.width: root.navFocus && root.navIndex === 3 ? 2 : 1
            border.color: root.navFocus && root.navIndex === 3 ? "#00a4dc" : "#243040"

            Canvas {
                id: gearIcon
                anchors.centerIn: parent
                width: 24
                height: 24
                property color tint: root.navFocus && root.navIndex === 3 ? "#ffffff" : "#c9d4df"
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

    // ------------------------------------------------------------------- SERVER
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "login"

        Column {
            anchors.centerIn: parent
            width: 640
            spacing: 16

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#ffffff"
                font.pixelSize: 30
                font.bold: true
                text: qsTr("Scegli il tuo server Jellyfin")
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#7f8c9b"
                font.pixelSize: 17
                wrapMode: Text.WordWrap
                text: app.searching
                      ? qsTr("Ricerca sulla rete locale…")
                      : (app.servers.length === 0
                         ? qsTr("Nessun server rilevato automaticamente.")
                         : qsTr("Premi A per connetterti."))
            }

            // server già salvato: si rientra senza ridigitare l'indirizzo
            Rectangle {
                width: parent.width
                height: 62
                radius: 8
                visible: root.savedServerRow() === 1
                color: root.serverIndex === 0 ? "#123243" : "#141e28"
                border.width: root.serverIndex === 0 ? 2 : 1
                border.color: root.serverIndex === 0 ? "#00a4dc" : "#1e2b38"

                Column {
                    anchors.left: parent.left
                    anchors.leftMargin: 16
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    Text { color: "#ffffff"; font.pixelSize: 20; text: qsTr("Server salvato") }
                    Text { color: "#6f8090"; font.pixelSize: 15; text: app.serverLabel }
                }
            }

            Repeater {
                model: app.servers
                delegate: Rectangle {
                    width: parent.width
                    height: 62
                    radius: 8
                    color: index + root.savedServerRow() === root.serverIndex ? "#123243" : "#141e28"
                    border.width: index + root.savedServerRow() === root.serverIndex ? 2 : 1
                    border.color: index + root.savedServerRow() === root.serverIndex ? "#00a4dc" : "#1e2b38"

                    Column {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2
                        Text { color: "#ffffff"; font.pixelSize: 20; text: modelData.name }
                        Text { color: "#6f8090"; font.pixelSize: 15; text: modelData.address }
                    }
                }
            }

            Rectangle {
                width: parent.width
                height: 52
                radius: 8
                color: root.serverIndex === app.servers.length + root.savedServerRow() ? "#123243" : "transparent"
                border.width: 1
                border.color: root.serverIndex === app.servers.length + root.savedServerRow() ? "#00a4dc" : "#1e2b38"

                Text {
                    anchors.centerIn: parent
                    color: "#9fb0c0"
                    font.pixelSize: 17
                    text: qsTr("Inserisci l'indirizzo del server…")
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#4a5a68"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                text: qsTr("A: seleziona    X/R: cerca in rete (LAN + UDP)    B: esci\nL'indirizzo si può anche mettere in\nsteamlink/config/system/jellyfin-server.txt sulla chiavetta")
            }
        }
    }

    // --------------------------------------------------------------------- AUTH
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "auth"

        Column {
            anchors.centerIn: parent
            width: 640
            spacing: 16

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#ffffff"
                font.pixelSize: 28
                font.bold: true
                text: qsTr("Accedi a %1").arg(app.serverLabel)
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#6f8090"
                font.pixelSize: 17
                text: app.serverLabel.length > 0 ? "" : app.serverLabel
            }

            // --- menu scelta metodo ---
            Column {
                width: parent.width
                spacing: 12
                visible: root.authStep === "menu"

                Rectangle {
                    width: parent.width
                    height: 62
                    radius: 8
                    color: root.authIndex === 0 ? "#123243" : "#141e28"
                    border.width: root.authIndex === 0 ? 2 : 1
                    border.color: root.authIndex === 0 ? "#00a4dc" : "#1e2b38"
                    Text {
                        anchors.centerIn: parent
                        color: "#ffffff"
                        font.pixelSize: 20
                        text: qsTr("Nome utente e password")
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 62
                    radius: 8
                    color: root.authIndex === 1 ? "#123243" : "#141e28"
                    border.width: root.authIndex === 1 ? 2 : 1
                    border.color: root.authIndex === 1 ? "#00a4dc" : "#1e2b38"
                    Text {
                        anchors.centerIn: parent
                        color: "#ffffff"
                        font.pixelSize: 20
                        text: qsTr("Quick Connect (codice a 6 cifre)")
                    }
                }

            }

            // --- credenziali ---
            Column {
                width: parent.width
                spacing: 12
                visible: root.authStep === "credentials"

                Rectangle {
                    width: parent.width
                    height: 62
                    radius: 8
                    color: root.authIndex === 0 ? "#123243" : "#141e28"
                    border.width: root.authIndex === 0 ? 2 : 1
                    border.color: root.authIndex === 0 ? "#00a4dc" : "#1e2b38"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        color: "#6f8090"
                        font.pixelSize: 14
                        text: qsTr("nome utente")
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        color: "#ffffff"
                        font.pixelSize: 20
                        text: root.credUser.length > 0 ? root.credUser : "—"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 62
                    radius: 8
                    color: root.authIndex === 1 ? "#123243" : "#141e28"
                    border.width: root.authIndex === 1 ? 2 : 1
                    border.color: root.authIndex === 1 ? "#00a4dc" : "#1e2b38"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.top: parent.top
                        anchors.topMargin: 8
                        color: "#6f8090"
                        font.pixelSize: 14
                        text: qsTr("password")
                    }
                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.bottom: parent.bottom
                        anchors.bottomMargin: 10
                        color: "#ffffff"
                        font.pixelSize: 20
                        text: root.credPass.length > 0 ? root.maskText(root.credPass) : "—"
                    }
                }

                Rectangle {
                    width: parent.width
                    height: 52
                    radius: 26
                    color: root.authIndex === 2 ? "#00a4dc" : "#123243"
                    border.width: root.authIndex === 2 ? 2 : 1
                    border.color: root.authIndex === 2 ? "#7fd9ff" : "#1e2b38"

                    Text {
                        anchors.centerIn: parent
                        color: root.authIndex === 2 ? "#00121a" : "#ffffff"
                        font.pixelSize: 19
                        font.bold: true
                        text: qsTr("Entra")
                    }
                }

            }
        }
    }

    // ------------------------------------------------------------ QUICK CONNECT
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "quickconnect"

        Column {
            anchors.centerIn: parent
            width: 760
            spacing: 14

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#ffffff"
                font.pixelSize: 28
                font.bold: true
                text: qsTr("Collega questo dispositivo")
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#00a4dc"
                font.pixelSize: 88
                font.bold: true
                font.letterSpacing: 12
                text: app.quickConnectCode.length > 0 ? app.quickConnectCode : "······"
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#9fb0c0"
                font.pixelSize: 18
                wrapMode: Text.WordWrap
                text: app.quickConnectStatus
            }

        }
    }

    // -------------------------------------------------------------------- HOME
    Flickable {
        id: homeFlick
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && page === "home"
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
                title: qsTr("Continua a guardare")
                items: app.resume
                landscape: true
                active: root.rowIndex === 0 && !root.navFocus
                currentIndex: root.colIndex
                visible: app.resume.length > 0
            }

            MediaRow {
                id: rowNextUp
                width: parent.width
                title: qsTr("Prossimo episodio")
                items: app.nextUp
                landscape: true
                active: root.rowIndex === 1 && !root.navFocus
                currentIndex: root.colIndex
                visible: app.nextUp.length > 0
            }

            MediaRow {
                id: rowLatest
                width: parent.width
                title: qsTr("Aggiunti di recente")
                items: app.latest
                showLibrary: true
                active: root.rowIndex === 2 && !root.navFocus
                currentIndex: root.colIndex
                visible: app.latest.length > 0
            }

            Item {
                id: rowLibs
                width: parent.width
                height: 150
                visible: app.views.length > 0

                Text {
                    id: libTitle
                    anchors.left: parent.left
                    anchors.leftMargin: 24
                    anchors.top: parent.top
                    color: root.rowIndex === 3 ? "#ffffff" : "#93a4b5"
                    font.pixelSize: 24
                    font.bold: true
                    text: qsTr("Librerie")
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
                    model: app.views
                    spacing: 14
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: 190
                        height: 90
                        radius: 10
                        color: root.rowIndex === 3 && index === root.colIndex && !root.navFocus ? "#123243" : "#141e28"
                        border.width: root.rowIndex === 3 && index === root.colIndex && !root.navFocus ? 2 : 1
                        border.color: root.rowIndex === 3 && index === root.colIndex && !root.navFocus ? "#00a4dc" : "#1e2b38"

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            color: "#ffffff"
                            font.pixelSize: 19
                            elide: Text.ElideRight
                            text: modelData.name
                        }
                    }
                }

                function ensureVisible() {
                    if (app.views.length === 0) return
                    libList.positionViewAtIndex(root.colIndex, ListView.Contain)
                }
            }
        }
    }

    // ---------------------------------------------------- HOME VUOTA / OFFLINE
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && root.page === "home" && root.homeEmpty() &&
                 app.statusMessage.length > 0
        z: 5

        Column {
            anchors.centerIn: parent
            width: 760
            spacing: 14

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#ff9a9a"
                font.pixelSize: 24
                font.bold: true
                wrapMode: Text.WordWrap
                text: app.statusMessage
            }
        }
    }

    // ----------------------------------------------------------------- LIBRERIA
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "library" || page === "favorites")

        Text {
            id: libHeader
            x: 24
            anchors.top: parent.top
            anchors.topMargin: 10
            color: "#ffffff"
            font.pixelSize: 28
            font.bold: true
            text: app.libraryTitle
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 24
            anchors.top: parent.top
            anchors.topMargin: 16
            color: "#5b6b7c"
            font.pixelSize: 15
            text: root.countItems(app.libraryItems.length)
        }

        GridView {
            id: libraryGrid
            anchors.top: libHeader.bottom
            anchors.topMargin: 12
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            cellWidth: 152
            cellHeight: 292
            clip: true
            model: app.libraryItems
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: libraryGrid.cellWidth
                height: libraryGrid.cellHeight

                PosterCard {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 150
                    title: modelData.name !== undefined ? modelData.name : ""
                    subtitle: modelData.subtitle !== undefined ? modelData.subtitle : ""
                    image: modelData.image !== undefined ? modelData.image : ""
                    progress: modelData.progress !== undefined ? modelData.progress : 0
                    active: index === root.gridIndex && !root.navFocus
                }
            }
        }
    }

    // ------------------------------------------------------- LIBRERIA VUOTA
    Text {
        anchors.centerIn: parent
        visible: app.state === "home" && (page === "library" || page === "favorites") &&
                 app.libraryItems.length === 0 && !app.loading
        horizontalAlignment: Text.AlignHCenter
        color: "#6f8090"
        font.pixelSize: 18
        text: page === "favorites" ? qsTr("Nessun preferito") : qsTr("Nessuna voce in questa libreria")
    }

    // -------------------------------------------------------- IMPOSTAZIONI / ACCOUNT
    Item {
        id: menuPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "settings" || page === "account")

        onVisibleChanged: {
            if (visible) menuList.positionViewAtIndex(0, ListView.Beginning)
        }

        // --- colonna sinistra: elenco
        Text {
            x: 40
            y: 12
            color: "#ffffff"
            font.pixelSize: 30
            font.bold: true
            text: page === "settings" ? qsTr("Impostazioni") : qsTr("Account")
        }
        Text {
            x: 40
            y: 52
            width: 540
            color: "#6f8090"
            font.pixelSize: 14
            elide: Text.ElideRight
            text: page === "settings"
                  ? qsTr("Percorso verso la versione stabile")
                  : (app.userName + "   ·   " + app.serverLabel)
        }

        ListView {
            id: menuList
            x: 40
            y: 90
            width: 540
            height: parent.height - 126
            clip: true
            model: page === "settings" ? root.settingsMenu : root.accountMenu
            spacing: 4
            boundsBehavior: Flickable.StopAtBounds

            delegate: Rectangle {
                width: menuList.width
                height: 48
                radius: 8
                color: index === root.menuIndex
                       ? "#123243"
                       : (index % 2 === 0 ? "#121c26" : "#101922")
                border.width: index === root.menuIndex ? 2 : 0
                border.color: "#00a4dc"

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 14
                    anchors.right: parent.right
                    anchors.rightMargin: root.menuValue(modelData).length > 0 ? 150 : 14
                    anchors.verticalCenter: parent.verticalCenter
                    color: index === root.menuIndex ? "#ffffff" : "#dbe5ef"
                    font.pixelSize: 16
                    font.bold: index === root.menuIndex
                    elide: Text.ElideRight
                    text: modelData.title
                }

                Text {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    color: "#7fd9ff"
                    font.pixelSize: 14
                    visible: root.menuValue(modelData).length > 0
                    text: root.menuValue(modelData)
                }
            }
        }

        // --- colonna destra: dettaglio della voce selezionata
        Rectangle {
            x: 620
            y: 16
            width: 620
            height: parent.height - 32
            radius: 16
            color: "#0e1620"
            border.width: 1
            border.color: "#1c2a38"

            Rectangle {
                x: 28
                y: 34
                width: 4
                height: 54
                radius: 2
                color: "#00a4dc"
                visible: page === "settings"
            }

            // profilo utente
            Row {
                x: 28
                y: 24
                spacing: 18
                visible: root.isProfileView()

                Item {
                    width: 84
                    height: 84

                    Rectangle {
                        anchors.fill: parent
                        radius: 42
                        color: "#182430"
                        border.width: 1
                        border.color: "#243040"
                    }
                    Text {
                        anchors.centerIn: parent
                        color: "#c9d4df"
                        font.pixelSize: 34
                        font.bold: true
                        text: app.userName.length > 0 ? app.userName.charAt(0).toUpperCase() : "?"
                    }
                    RoundedImage {
                        anchors.fill: parent
                        radius: 42
                        source: app.userImage
                        sourceSizeWidth: 168
                    }
                }

                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4
                    Text {
                        color: "#ffffff"
                        font.pixelSize: 24
                        font.bold: true
                        text: app.userName
                    }
                    Text {
                        color: "#6f8090"
                        font.pixelSize: 14
                        text: app.serverLabel
                    }
                }
            }

            // statistiche
            Row {
                x: 28
                y: 124
                spacing: 10
                visible: root.isProfileView()

                Rectangle {
                    width: 130; height: 64; radius: 10
                    color: "#121c26"; border.width: 1; border.color: "#1c2a38"
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#7fd9ff"; font.pixelSize: 20; font.bold: true
                               text: app.userStats.movies !== undefined ? app.userStats.movies : "–" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#6f8090"; font.pixelSize: 12; text: qsTr("Film") }
                    }
                }
                Rectangle {
                    width: 130; height: 64; radius: 10
                    color: "#121c26"; border.width: 1; border.color: "#1c2a38"
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#7fd9ff"; font.pixelSize: 20; font.bold: true
                               text: app.userStats.series !== undefined ? app.userStats.series : "–" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#6f8090"; font.pixelSize: 12; text: qsTr("Serie") }
                    }
                }
                Rectangle {
                    width: 130; height: 64; radius: 10
                    color: "#121c26"; border.width: 1; border.color: "#1c2a38"
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#7fd9ff"; font.pixelSize: 20; font.bold: true
                               text: app.userStats.episodes !== undefined ? app.userStats.episodes : "–" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#6f8090"; font.pixelSize: 12; text: qsTr("Episodi") }
                    }
                }
                Rectangle {
                    width: 130; height: 64; radius: 10
                    color: "#121c26"; border.width: 1; border.color: "#1c2a38"
                    Column {
                        anchors.centerIn: parent
                        spacing: 2
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#7fd9ff"; font.pixelSize: 20; font.bold: true
                               text: app.userStats.favorites !== undefined ? app.userStats.favorites : "–" }
                        Text { anchors.horizontalCenter: parent.horizontalCenter
                               color: "#6f8090"; font.pixelSize: 12; text: qsTr("Preferiti") }
                    }
                }
            }

            Rectangle {
                x: 28
                y: 202
                width: parent.width - 56
                height: 1
                color: "#1c2a38"
                visible: root.isProfileView()
            }

            Column {
                x: 48
                y: 28
                width: parent.width - 88
                spacing: 10
                visible: !root.isProfileView()

                Text {
                    width: parent.width
                    color: "#ffffff"
                    font.pixelSize: 28
                    font.bold: true
                    wrapMode: Text.WordWrap
                    text: root.currentMenuEntry().title
                }
                Text {
                    width: parent.width
                    color: "#7fd9ff"
                    font.pixelSize: 22
                    font.bold: true
                    visible: root.menuValue(root.currentMenuEntry()).length > 0
                    text: root.menuValue(root.currentMenuEntry())
                }
                Text {
                    width: parent.width
                    color: "#8fa3b6"
                    font.pixelSize: 15
                    lineHeight: 1.2
                    wrapMode: Text.WordWrap
                    text: root.currentMenuEntry().desc
                }
            }

            Text {
                anchors.left: parent.left
                anchors.leftMargin: 28
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 22
                color: "#3f4c58"
                font.pixelSize: 13
                text: "PolypLink " + appVersion
            }
        }
    }

    // ------------------------------------------------------------------ STAGIONE
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && root.page === "episodes" && !root.detailOpen

        Row {
            id: seasonDetailHeader
            anchors.top: parent.top
            anchors.topMargin: 8
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            spacing: 20

            RoundedImage {
                width: 110
                height: 190
                radius: 8
                source: (app.seasonInfo.image !== undefined && app.seasonInfo.image.length > 0)
                        ? app.seasonInfo.image : app.seriesPoster
                sourceSizeWidth: 300
            }

            Column {
                width: parent.width - 130
                spacing: 8

                Text {
                    width: parent.width
                    color: "#ffffff"
                    font.pixelSize: 26
                    font.bold: true
                    elide: Text.ElideRight
                    text: app.seasonTitle
                }

                Text {
                    width: parent.width
                    color: "#6f8090"
                    font.pixelSize: 15
                    elide: Text.ElideRight
                    text: app.seriesTitle + "   ·   " + root.countEpisodes(app.episodes.length)
                }

                Row {
                    spacing: 10

                    ActionButton {
                        icon: "replay"
                        label: qsTr("Guarda dall'inizio")
                        primary: true
                        active: root.seasonActionsFocus && root.seasonAction === 0
                    }
                    ActionButton {
                        icon: (app.seasonInfo.played !== undefined && app.seasonInfo.played)
                              ? "check-double" : "check"
                        label: (app.seasonInfo.played !== undefined && app.seasonInfo.played)
                               ? qsTr("Segna come non visto") : qsTr("Segna come visto")
                        active: root.seasonActionsFocus && root.seasonAction === 1
                    }
                    ActionButton {
                        icon: (app.seasonInfo.favorite !== undefined && app.seasonInfo.favorite)
                              ? "heart-filled" : "heart-empty"
                        customIconColor: (app.seasonInfo.favorite !== undefined && app.seasonInfo.favorite)
                                         ? "#ff4d4d" : "#ffffff"
                        label: (app.seasonInfo.favorite !== undefined && app.seasonInfo.favorite)
                               ? qsTr("Togli preferito") : qsTr("Preferito")
                        active: root.seasonActionsFocus && root.seasonAction === 2
                    }
                }

                Text {
                    width: parent.width
                    height: 40
                    color: "#8fa3b6"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: app.seasonInfo.overview !== undefined ? app.seasonInfo.overview : ""
                }
            }
        }

        ListView {
            id: episodeList
            anchors.top: seasonDetailHeader.bottom
            anchors.topMargin: 10
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            clip: true
            model: app.episodes
            spacing: 8
            boundsBehavior: Flickable.StopAtBounds

            delegate: Item {
                width: episodeList.width
                height: 112

                Rectangle {
                    anchors.fill: parent
                    radius: 8
                    color: index === root.epIndex && !root.seasonActionsFocus ? "#123243" : "#101920"
                    border.width: index === root.epIndex && !root.seasonActionsFocus ? 2 : 1
                    border.color: index === root.epIndex && !root.seasonActionsFocus ? "#00a4dc" : "#1b2733"
                }

                RoundedImage {
                    x: 10
                    anchors.verticalCenter: parent.verticalCenter
                    width: 150
                    height: 84
                    radius: 7
                    source: modelData.image !== undefined ? modelData.image : ""
                    sourceSizeWidth: 300
                }

                Text {
                    id: epRowTitle
                    anchors.left: parent.left
                    anchors.leftMargin: 174
                    anchors.right: parent.right
                    anchors.rightMargin: 110
                    anchors.top: parent.top
                    anchors.topMargin: 12
                    color: "#ffffff"
                    font.pixelSize: 19
                    font.bold: index === root.epIndex && !root.seasonActionsFocus
                    elide: Text.ElideRight
                    text: {
                        var n = modelData.indexNumber !== undefined ? modelData.indexNumber : 0
                        var sn = modelData.parentIndexNumber !== undefined ? modelData.parentIndexNumber : 0
                        var label = (sn > 0 || n > 0) ? ("S" + sn + " E" + n + "  ·  ") : ""
                        return label + (modelData.name !== undefined ? modelData.name : "")
                    }
                }

                Text {
                    anchors.left: parent.left
                    anchors.leftMargin: 174
                    anchors.right: parent.right
                    anchors.rightMargin: 110
                    anchors.top: epRowTitle.bottom
                    anchors.topMargin: 3
                    color: "#8fa3b6"
                    font.pixelSize: 14
                    wrapMode: Text.WordWrap
                    maximumLineCount: 2
                    elide: Text.ElideRight
                    text: modelData.overview !== undefined ? modelData.overview : ""
                }

                Rectangle {
                    anchors.right: parent.right
                    anchors.rightMargin: 14
                    anchors.verticalCenter: parent.verticalCenter
                    width: 78
                    height: 22
                    radius: 4
                    color: "#0b0f14"
                    border.width: 1
                    border.color: "#2b6f8c"
                    visible: modelData.played !== undefined && modelData.played
                    Text { anchors.centerIn: parent; color: "#7fd9ff"; font.pixelSize: 12; text: qsTr("VISTO") }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.leftMargin: 174
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: 8
                    width: 220
                    height: 4
                    radius: 2
                    color: "#22303d"
                    visible: modelData.progress !== undefined && modelData.progress > 0

                    Rectangle {
                        height: parent.height
                        radius: 2
                        width: parent.width * Math.min(1, (modelData.progress !== undefined ? modelData.progress : 0) / 100.0)
                        color: "#00a4dc"
                    }
                }
            }
        }
    }

    // -------------------------------------------------------- STAGIONE VUOTA
    Text {
        anchors.centerIn: parent
        visible: app.state === "home" && page === "episodes" && !root.detailOpen &&
                 app.episodes.length === 0 && !app.loading
        horizontalAlignment: Text.AlignHCenter
        color: "#6f8090"
        font.pixelSize: 18
        text: qsTr("Nessun episodio in questa stagione")
    }

    // ----------------------------------------------------------------- STAGIONI
    Item {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && root.page === "seasons"

        Text {
            id: seasonHeader
            x: 24
            anchors.top: parent.top
            anchors.topMargin: 10
            color: "#ffffff"
            font.pixelSize: 26
            font.bold: true
            text: app.seriesTitle
        }

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.top: seasonHeader.bottom
            anchors.topMargin: 2
            color: "#6f8090"
            font.pixelSize: 15
            text: root.countSeasons(app.seasons.length)
        }

        GridView {
            id: seasonGrid
            anchors.top: seasonHeader.bottom
            anchors.topMargin: 34
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.leftMargin: 24
            anchors.rightMargin: 24
            clip: true
            // stessa griglia dei film: locandina verticale anche per le stagioni
            cellWidth: 152
            cellHeight: 292
            boundsBehavior: Flickable.StopAtBounds
            model: app.seasons

            delegate: Item {
                width: seasonGrid.cellWidth
                height: seasonGrid.cellHeight

                PosterCard {
                    anchors.horizontalCenter: parent.horizontalCenter
                    width: 150
                    title: modelData.name !== undefined ? modelData.name : ""
                    subtitle: root.countEpisodes(modelData.childCount !== undefined ? modelData.childCount : 0)
                    image: (modelData.image !== undefined && modelData.image.length > 0)
                           ? modelData.image : app.seriesPoster
                    played: modelData.played !== undefined ? modelData.played : false
                    active: index === root.seasonIndex
                }
            }
        }
    }

    // --------------------------------------------------------- STAGIONI VUOTE
    Text {
        anchors.centerIn: parent
        visible: app.state === "home" && page === "seasons" &&
                 app.seasons.length === 0 && !app.loading
        horizontalAlignment: Text.AlignHCenter
        color: "#6f8090"
        font.pixelSize: 18
        text: qsTr("Nessuna stagione")
    }

    // ----------------------------------------------------------------- DETTAGLIO
    Item {
        anchors.fill: parent
        visible: root.detailOpen
        z: 10

        Image {
            anchors.fill: parent
            source: app.detail.backdrop !== undefined ? app.detail.backdrop : ""
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            cache: true
            visible: status === Image.Ready
        }

        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#cc0b0f14" }
                GradientStop { position: 0.55; color: "#990b0f14" }
                GradientStop { position: 1.0; color: "#f20b0f14" }
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
                image: app.detail.image !== undefined ? app.detail.image : ""
                progress: app.detail.progress !== undefined ? app.detail.progress : 0
                played: app.detail.played !== undefined ? app.detail.played : false
                active: false
            }

            Column {
                width: parent.width - 200
                spacing: 10

                Text {
                    width: parent.width
                    color: "#ffffff"
                    font.pixelSize: 30
                    font.bold: true
                    elide: Text.ElideRight
                    text: app.detail.name !== undefined ? app.detail.name : ""
                }

                Text {
                    width: parent.width
                    color: "#9fb0c0"
                    font.pixelSize: 15
                    text: {
                        var parts = []
                        if (app.detail.subtitle !== undefined && app.detail.subtitle.length > 0)
                            parts.push(app.detail.subtitle)
                        if (app.detail.officialRating !== undefined && app.detail.officialRating.length > 0)
                            parts.push(app.detail.officialRating)
                        if (app.detail.communityRating !== undefined && app.detail.communityRating > 0)
                            parts.push("★ " + app.detail.communityRating.toFixed(1))
                        return parts.join("   ·   ")
                    }
                }

                // azioni: solo icona, si allargano con il focus
                Row {
                    spacing: 10

                    ActionButton {
                        icon: "play"
                        label: (root.hasResume() && !root.isSeries()) ? qsTr("Riprendi") : qsTr("Riproduci")
                        active: root.detailFocus === 0
                        primary: true
                    }
                    ActionButton {
                        icon: "series"
                        label: qsTr("Stagioni")
                        visible: root.isSeries()
                        active: root.detailFocus === 1
                    }
                    ActionButton {
                        icon: "replay"
                        label: qsTr("Riproduci dall'inizio")
                        visible: root.hasResume() && !root.isSeries()
                        active: root.detailFocus === 1
                    }
                    ActionButton {
                        icon: (app.detail.played !== undefined && app.detail.played)
                              ? "check-double" : "check"
                        label: (app.detail.played !== undefined && app.detail.played)
                               ? qsTr("Segna come non visto") : qsTr("Segna come visto")
                        active: root.detailFocus === root.playedIndex()
                    }
                    ActionButton {
                        icon: (app.detail.favorite !== undefined && app.detail.favorite)
                              ? "heart-filled" : "heart-empty"
                        customIconColor: (app.detail.favorite !== undefined && app.detail.favorite)
                                         ? "#ff4d4d" : "#ffffff"
                        label: (app.detail.favorite !== undefined && app.detail.favorite)
                               ? qsTr("Togli preferito") : qsTr("Preferito")
                        active: root.detailFocus === root.favoriteIndex()
                    }
                    ActionButton {
                        icon: "series"
                        label: qsTr("Vai alla serie completa")
                        visible: root.hasSeriesLink()
                        active: root.detailFocus === root.seriesIndex()
                    }
                    ActionButton {
                        icon: "previous"
                        label: qsTr("Episodio precedente")
                        visible: root.isEpisode()
                        active: root.detailFocus === root.previousIndex()
                    }
                    ActionButton {
                        icon: "text"
                        label: qsTr("Leggi tutto")
                        visible: root.hasOverview()
                        active: root.detailFocus === root.readMoreIndex()
                    }
                }

                Text {
                    width: parent.width
                    height: 78
                    color: "#c9d4df"
                    font.pixelSize: 15
                    wrapMode: Text.WordWrap
                    elide: Text.ElideRight
                    text: app.detail.overview !== undefined ? app.detail.overview : ""
                }

                // impostazioni di riproduzione
                Column {
                    spacing: 8
                    visible: !root.isSeries()

                    Rectangle {
                        width: 520; height: 40; radius: 8
                        color: root.detailFocus === root.settingsStart() ? "#123243" : "#101920"
                        border.width: 1
                        border.color: root.detailFocus === root.settingsStart() ? "#00a4dc" : "#1b2733"
                        Text { anchors.left: parent.left; anchors.leftMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#9fb0c0"; font.pixelSize: 15; text: qsTr("Qualità") }
                        Text { anchors.right: parent.right; anchors.rightMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#ffffff"; font.pixelSize: 15
                               text: (app.qualityKbps / 1000) + " Mbps" }
                    }

                    Rectangle {
                        width: 520; height: 40; radius: 8
                        color: root.detailFocus === root.settingsStart() + 1 ? "#123243" : "#101920"
                        border.width: 1
                        border.color: root.detailFocus === root.settingsStart() + 1 ? "#00a4dc" : "#1b2733"
                        Text { anchors.left: parent.left; anchors.leftMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#9fb0c0"; font.pixelSize: 15; text: qsTr("Audio") }
                        Text { anchors.right: parent.right; anchors.rightMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#ffffff"; font.pixelSize: 15
                               text: root.audioLabel() }
                    }

                    Rectangle {
                        width: 520; height: 40; radius: 8
                        color: root.detailFocus === root.settingsStart() + 2 ? "#123243" : "#101920"
                        border.width: 1
                        border.color: root.detailFocus === root.settingsStart() + 2 ? "#00a4dc" : "#1b2733"
                        Text { anchors.left: parent.left; anchors.leftMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#9fb0c0"; font.pixelSize: 15; text: qsTr("Sottotitoli") }
                        Text { anchors.right: parent.right; anchors.rightMargin: 14
                               anchors.verticalCenter: parent.verticalCenter
                               color: "#ffffff"; font.pixelSize: 15
                               text: root.subtitleLabel() }
                    }
                }

                Text {
                    width: parent.width
                    color: "#7f8c9b"
                    font.pixelSize: 13
                    elide: Text.ElideRight
                    text: root.metaLine(app.detail)
                }
            }
        }
    }

    // ------------------------------------------------------------------ tastiera
    Item {
        anchors.fill: parent
        visible: root.keyboardOpen
        z: 30

        Rectangle {
            anchors.fill: parent
            color: "#c0000000"
        }

        Rectangle {
            anchors.centerIn: parent
            width: 1040
            height: 470
            radius: 14
            color: "#0f1821"
            border.width: 1
            border.color: "#243040"

            Column {
                anchors.centerIn: parent
                spacing: 14

                Rectangle {
                    width: 1000
                    height: 46
                    radius: 7
                    color: "#0a1118"
                    border.width: 1
                    border.color: "#2a3b4b"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        color: "#ffffff"
                        font.pixelSize: 21
                        text: root.kbTarget === "pass"
                              ? root.maskText(keyboard.text) : keyboard.text
                        elide: Text.ElideLeft
                    }
                }

                Keyboard {
                    id: keyboard

                    onAccepted: {
                        if (root.kbTarget === "url") app.setManualServer(value)
                        else if (root.kbTarget === "user") root.credUser = value
                        else if (root.kbTarget === "pass") root.credPass = value
                        root.keyboardOpen = false
                    }
                    onCancelled: root.keyboardOpen = false
                }

            }
        }
    }

    // -------------------------------------------------------------------- stato
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 30
        color: "#0d151d"
        visible: app.statusMessage.length > 0 || app.loading

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            color: "#8fa3b6"
            font.pixelSize: 15
            text: app.loading ? qsTr("caricamento…") : app.statusMessage
        }
    }


    // ------------------------------------------------------------------- toast
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: app.playerActive ? 260 : 44
        width: toastLabel.implicitWidth + 40
        height: 38
        radius: 19
        color: "#e6123243"
        border.width: 1
        border.color: "#00a4dc"
        opacity: app.notice.length > 0 ? 1 : 0
        visible: opacity > 0
        z: 30
        Behavior on opacity { NumberAnimation { duration: 250 } }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            color: "#e6f7ff"
            font.pixelSize: 15
            text: app.notice
        }
    }

    // ---------------------------------------------------------- TRAMA COMPLETA
    Item {
        anchors.fill: parent
        visible: root.overviewOpen && root.detailOpen
        z: 50

        Rectangle {
            anchors.fill: parent
            color: "#f20b0f14"
        }

        Column {
            anchors.fill: parent
            anchors.margins: 60
            spacing: 16

            Text {
                width: parent.width
                color: "#ffffff"
                font.pixelSize: 28
                font.bold: true
                elide: Text.ElideRight
                text: app.detail.name !== undefined ? app.detail.name : ""
            }

            Flickable {
                id: overviewFlick
                width: parent.width
                height: parent.height - 40
                contentWidth: width
                contentHeight: overviewFullText.height
                clip: true
                boundsBehavior: Flickable.StopAtBounds

                Text {
                    id: overviewFullText
                    width: overviewFlick.width
                    color: "#dbe5ef"
                    font.pixelSize: 18
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                    text: app.detail.overview !== undefined ? app.detail.overview : ""
                }
            }
        }
    }

    }

    // ------------------------------------------------------- LINGUA INTERFACCIA
    Item {
        anchors.fill: parent
        visible: root.languageOpen
        z: 60

        Rectangle {
            anchors.fill: parent
            color: "#f20b0f14"
        }

        Column {
            anchors.centerIn: parent
            spacing: 18

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: "#ffffff"
                font.pixelSize: 28
                font.bold: true
                text: qsTr("Lingua dell'interfaccia")
            }

            GridView {
                id: languageGrid
                width: 1110
                height: 454
                cellWidth: 370
                cellHeight: 62
                interactive: false
                clip: true
                model: language.available

                delegate: Item {
                    width: languageGrid.cellWidth
                    height: languageGrid.cellHeight

                    Rectangle {
                        width: 350
                        height: 54
                        anchors.centerIn: parent
                        radius: 27
                        color: index === root.languageIndex ? "#123243" : "#141e28"
                        border.width: index === root.languageIndex ? 2 : 1
                        border.color: index === root.languageIndex ? "#00a4dc" : "#1e2b38"

                        Text {
                            anchors.centerIn: parent
                            color: index === root.languageIndex ? "#ffffff" : "#dbe5ef"
                            font.pixelSize: 19
                            font.bold: index === root.languageIndex
                            text: modelData.name
                        }
                    }
                }
            }
        }
    }

    // ------------------------------------------------------------------ PLAYER
    Item {
        id: playerLayer
        visible: app.playerActive
        width: 1280
        height: 720
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.TopLeft
        x: Math.round((root.width - width * scale) / 2)
        y: Math.round((root.height - height * scale) / 2)

        // OSD del player, sul modello Jellyfin: barra di seek con cursore,
        // pulsanti di trasporto e tracce. Sfuma dopo 5 s.
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
                    GradientStop { position: 0.0; color: "#00000000" }
                    GradientStop { position: 1.0; color: "#e6000000" }
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
                color: "#ffffff"
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
                color: app.seekPending ? "#ffffff" : "#c9d4df"
                font.pixelSize: 18
                text: root.fmtTime(app.seekPending ? app.seekPreview : app.playerPosition) +
                      "  /  " + root.fmtTime(app.playerDuration)
            }

            // barra di seek: bianco = destinazione, azzurro = gia' visto
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
                    color: "#2200a4dc"
                    visible: app.playerFocus === 0 && app.osdVisible
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width
                    height: 7
                    radius: 3
                    color: "#55ffffff"
                }

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    width: app.playerDuration > 0
                           ? parent.width * Math.min(1, (app.seekPending ? app.seekPreview : app.playerPosition) / app.playerDuration)
                           : 0
                    height: 7
                    radius: 3
                    color: "#00a4dc"
                }

                Rectangle {
                    id: osdKnob
                    width: 8
                    height: 24
                    radius: 4
                    color: app.seekPending ? "#ffffff" : "#7fd9ff"
                    anchors.verticalCenter: parent.verticalCenter
                    x: app.playerDuration > 0
                       ? Math.max(0, Math.min(parent.width - width,
                             parent.width * (app.seekPending ? app.seekPreview : app.playerPosition) / app.playerDuration - width / 2))
                       : 0
                }

                // fumetto con il tempo di destinazione
                Rectangle {
                    visible: app.seekPending
                    width: 120
                    height: 34
                    radius: 8
                    color: "#f20b0f14"
                    border.width: 1
                    border.color: "#00a4dc"
                    anchors.bottom: parent.top
                    anchors.bottomMargin: 4
                    x: Math.max(0, Math.min(parent.width - width,
                          osdKnob.x + osdKnob.width / 2 - width / 2))

                    Text {
                        anchors.centerIn: parent
                        color: "#ffffff"
                        font.pixelSize: 16
                        font.bold: true
                        text: root.fmtTime(app.seekPreview)
                    }
                }

                // focus sulla barra
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -8
                    radius: 14
                    color: "transparent"
                    border.width: app.playerFocus === 0 && app.osdVisible ? 2 : 0
                    border.color: "#00a4dc"
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
                PlayerButton { text: qsTr("Audio"); active: app.playerFocus === 4 }
                PlayerButton { text: qsTr("Sottotitoli"); active: app.playerFocus === 5 }
                PlayerButton { text: qsTr("Qualità"); active: app.playerFocus === 6 }
                PlayerButton { text: qsTr("Statistiche"); active: app.playerFocus === 7 }
            }

        }

        // ---------------------------------------- MENU AUDIO/SOTTOTITOLI/QUALITA'
        Rectangle {
            id: playerMenu
            visible: app.playerMenuOpen
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 96
            width: 520
            height: Math.min(430, 52 + app.playerMenuItems.length * 42)
            radius: 14
            color: "#ee0b0f14"
            border.width: 2
            border.color: "#00a4dc"
            z: 12

            Text {
                id: playerMenuTitle
                anchors.left: parent.left
                anchors.leftMargin: 18
                anchors.top: parent.top
                anchors.topMargin: 12
                color: "#7fd9ff"
                font.pixelSize: 16
                font.bold: true
                text: app.playerMenuMode === "audio" ? qsTr("Audio") :
                      (app.playerMenuMode === "subtitles" ? qsTr("Sottotitoli") : qsTr("Qualità"))
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
                    color: index === app.playerMenuIndex ? "#123243" : "transparent"
                    border.width: index === app.playerMenuIndex ? 2 : 0
                    border.color: "#00a4dc"

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.right: parent.right
                        anchors.rightMargin: 28
                        anchors.verticalCenter: parent.verticalCenter
                        color: index === app.playerMenuIndex ? "#ffffff" : "#dbe5ef"
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
                        color: "#7fd9ff"
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

        // ---------------------------------------------------- CONNESSIONE PERSA
        Rectangle {
            id: stalledCard
            visible: app.playerStalled && app.playerActive
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 56
            width: 620
            height: 142
            radius: 16
            color: "#ee0b0f14"
            border.width: 2
            border.color: "#ff6b6b"
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
                        color: "#ff8a8a"
                        font.pixelSize: 23
                        font.bold: true
                        text: app.playerStatus
                    }

                    Rectangle {
                        width: 150
                        height: 34
                        radius: 17
                        color: "#2a1216"
                        border.width: 1
                        border.color: "#ff6b6b"

                        Text {
                            anchors.centerIn: parent
                            color: "#ffb3b3"
                            font.pixelSize: 15
                            font.bold: true
                            text: qsTr("Riprendi")
                        }
                    }
                }
            }
        }

        // ---------------------------------------------------- PROSSIMO EPISODIO
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
            color: "#ee0b0f14"
            border.width: 2
            border.color: "#00a4dc"
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
                        color: "#7fd9ff"
                        font.pixelSize: 14
                        font.bold: true
                        text: qsTr("Prossimo episodio")
                    }
                    Text {
                        width: parent.width
                        color: "#ffffff"
                        font.pixelSize: 23
                        font.bold: true
                        elide: Text.ElideRight
                        text: app.nextEpisode.name !== undefined ? app.nextEpisode.name : ""
                    }
                    Text {
                        width: parent.width
                        color: "#9fb0c0"
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
                            color: "#7fd9ff"
                            font.pixelSize: 16
                            font.bold: true
                            text: qsTr("Riproduci")
                        }
                    }
                }
            }
        }

        // simbolo pausa: due barre, nessun glifo da font
        Row {
            anchors.centerIn: parent
            spacing: 14
            visible: app.playerPaused

            Rectangle { width: 16; height: 52; radius: 3; color: "#ffffff" }
            Rectangle { width: 16; height: 52; radius: 3; color: "#ffffff" }
        }

        // statistiche di transcodifica (tasto Start/Menu)
        Rectangle {
            visible: app.statsOpen
            z: 6
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.margins: 24
            width: 470
            height: statsColumn.height + 28
            radius: 10
            color: "#cc0b0f14"
            border.width: 1
            border.color: "#2b6f8c"

            Column {
                id: statsColumn
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 14
                spacing: 4

                Text {
                    color: "#7fd9ff"
                    font.pixelSize: 16
                    font.bold: true
                    text: qsTr("Transcodifica (server)")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Risoluzione   %1").arg(app.playerStats.resolution !== undefined ? app.playerStats.resolution : "?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: app.playerStats.bandwidth !== undefined
                          ? qsTr("Bitrate       %1 Mbps (richiesti 12)").arg((app.playerStats.bandwidth / 1000000).toFixed(1))
                          : qsTr("Bitrate       %1").arg("?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Codec         %1").arg(app.playerStats.codecs !== undefined ? app.playerStats.codecs : "?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("FPS inviati   %1").arg(app.playerStats.fps !== undefined ? app.playerStats.fps : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Coda video    %1 frame").arg(app.playerStats.queueVideo !== undefined ? app.playerStats.queueVideo : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Coda audio    %1 ms").arg(app.playerStats.queueAudio !== undefined
                          ? Math.round(app.playerStats.queueAudio / 48) : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Segmento      %1 ms (download+demux)").arg(app.playerStats.segmentMs !== undefined ? app.playerStats.segmentMs : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Frame rifiutati  %1").arg(app.playerStats.failed !== undefined ? app.playerStats.failed : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: qsTr("Scartati (ritardo)  %1").arg(app.playerStats.dropped !== undefined ? app.playerStats.dropped : 0)
                }
            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 40
            visible: app.playerStatus.length > 0 && !app.playerStalled
            color: "#ff6b6b"
            font.pixelSize: 20
            text: app.playerStatus
        }
    }

    // -------------------------------------------------------------------- input
    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            // Con una tastiera USB i tasti arrivano qui (il controller passa
            // da main.cpp): stesso OSD, stessi comandi.
            if (app.playerActive) {
                if (event.key === Qt.Key_Return) { app.playerConfirm() }
                else if (event.key === Qt.Key_Left) { app.playerNavigate(-1, 0) }
                else if (event.key === Qt.Key_Right) { app.playerNavigate(1, 0) }
                else if (event.key === Qt.Key_Up) { app.playerNavigate(0, -1) }
                else if (event.key === Qt.Key_Down) { app.playerNavigate(0, 1) }
                else if (event.key === Qt.Key_Escape) { app.playerBack() }
                else if (event.key === Qt.Key_Menu) { app.toggleStats() }
                event.accepted = true
                return
            }

            // tastiera aperta: tutto va a lei
            if (root.keyboardOpen) {
                if (event.key === Qt.Key_Up) { keyboard.move(0, -1); event.accepted = true; return }
                if (event.key === Qt.Key_Down) { keyboard.move(0, 1); event.accepted = true; return }
                if (event.key === Qt.Key_Left) { keyboard.move(-1, 0); event.accepted = true; return }
                if (event.key === Qt.Key_Right) { keyboard.move(1, 0); event.accepted = true; return }
                if (event.key === Qt.Key_Return) { keyboard.press(); event.accepted = true; return }
                if (event.key === Qt.Key_Backspace) { keyboard.backspace(); event.accepted = true; return }
                if (event.key === Qt.Key_Escape) { root.keyboardOpen = false; event.accepted = true; return }
                if (event.text.length === 1) { keyboard.appendChar(event.text); event.accepted = true; return }
                event.accepted = false
                return
            }

            // barra superiore: sinistra/destra scelgono, A apre, giu' torna al contenuto
            if (app.state === "home" && root.navFocus) {
                if (event.key === Qt.Key_Left) { root.navIndex = Math.max(0, root.navIndex - 1); event.accepted = true; return }
                if (event.key === Qt.Key_Right) { root.navIndex = Math.min(3, root.navIndex + 1); event.accepted = true; return }
                if (event.key === Qt.Key_Down) {
                    root.navFocus = false
                    if (page === "home") focusContent()
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Return) { activateNav(); event.accepted = true; return }
                if (event.key === Qt.Key_Escape) { root.navFocus = false; event.accepted = true; return }
                event.accepted = false
                return
            }

            if (root.languageOpen) {
                var langCols = 3
                if (event.key === Qt.Key_Left) {
                    root.languageIndex = Math.max(0, root.languageIndex - 1)
                } else if (event.key === Qt.Key_Right) {
                    root.languageIndex = Math.min(language.available.length - 1,
                                                  root.languageIndex + 1)
                } else if (event.key === Qt.Key_Up) {
                    root.languageIndex = Math.max(0, root.languageIndex - langCols)
                } else if (event.key === Qt.Key_Down) {
                    root.languageIndex = Math.min(language.available.length - 1,
                                                  root.languageIndex + langCols)
                } else if (event.key === Qt.Key_Return) {
                    if (root.languageIndex >= 0 &&
                        root.languageIndex < language.available.length) {
                        language.setCurrent(language.available[root.languageIndex].code)
                    }
                    root.languageOpen = false
                } else if (event.key === Qt.Key_Escape ||
                           event.key === Qt.Key_Left ||
                           event.key === Qt.Key_Backspace) {
                    root.languageOpen = false
                }
                event.accepted = true
                return
            }

            if (root.overviewOpen) {
                if (event.key === Qt.Key_Escape || event.key === Qt.Key_Return ||
                    event.key === Qt.Key_Left || event.key === Qt.Key_Backspace) {
                    root.overviewOpen = false
                } else if (event.key === Qt.Key_Down) {
                    overviewFlick.contentY = Math.min(
                        Math.max(0, overviewFlick.contentHeight - overviewFlick.height),
                        overviewFlick.contentY + 120)
                } else if (event.key === Qt.Key_Up) {
                    overviewFlick.contentY = Math.max(0, overviewFlick.contentY - 120)
                }
                event.accepted = true
                return
            }

            var handled = true

            if (app.state === "login") {
                var serverTotal = app.servers.length + savedServerRow()
                if (event.key === Qt.Key_Up) { serverIndex = Math.max(0, serverIndex - 1) }
                else if (event.key === Qt.Key_Down) { serverIndex = Math.min(serverTotal, serverIndex + 1) }
                else if (event.key === Qt.Key_Return) { activate() }
                else if (event.key === Qt.Key_R || event.key === Qt.Key_X) { app.discover() }
                else if (event.key === Qt.Key_Escape) { Qt.quit() }
                else handled = false
            } else if (app.state === "auth") {
                if (authStep === "menu") {
                    if (event.key === Qt.Key_Up) { authIndex = 0 }
                    else if (event.key === Qt.Key_Down) { authIndex = 1 }
                    else if (event.key === Qt.Key_Return) { activate() }
                    else if (event.key === Qt.Key_Escape) { goBack() }
                    else handled = false
                } else {
                    if (event.key === Qt.Key_Up) { authIndex = Math.max(0, authIndex - 1) }
                    else if (event.key === Qt.Key_Down) { authIndex = Math.min(2, authIndex + 1) }
                    else if (event.key === Qt.Key_Return) { activate() }
                    else if (event.key === Qt.Key_Escape) { goBack() }
                    else handled = false
                }
            } else if (app.state === "quickconnect") {
                if (event.key === Qt.Key_Escape) { goBack() }
                else handled = false
            } else if (detailOpen) {
                var s = root.settingsStart()
                var lastAction = (s >= 0) ? s - 1
                                 : (root.readMoreIndex() >= 0 ? root.readMoreIndex() : 3)
                if (event.key === Qt.Key_Up) {
                    if (s >= 0 && detailFocus > s) detailFocus = detailFocus - 1
                    else if (s >= 0 && detailFocus === s) detailFocus = root.favoriteIndex()
                    else detailFocus = Math.max(0, detailFocus - 1)
                } else if (event.key === Qt.Key_Down) {
                    if (s >= 0) {
                        if (detailFocus < s) detailFocus = s
                        else detailFocus = Math.min(s + 2, detailFocus + 1)
                    }
                } else if (event.key === Qt.Key_Left || event.key === Qt.Key_Right) {
                    var d = (event.key === Qt.Key_Left) ? -1 : 1
                    if (s < 0 || detailFocus < s) detailFocus = Math.max(0, Math.min(lastAction, detailFocus + d))
                    else if (detailFocus === s) app.cycleQuality(d)
                    else if (detailFocus === s + 1) app.cycleAudio(d)
                    else app.cycleSubtitle(d)
                } else if (event.key === Qt.Key_Return) {
                    detailAction()
                } else if (event.key === Qt.Key_Escape) { goBack() }
                else handled = false
            } else if (page === "seasons") {
                var scols = Math.max(1, Math.floor(seasonGrid.width / seasonGrid.cellWidth))
                if (event.key === Qt.Key_Left) {
                    seasonIndex = Math.max(0, seasonIndex - 1)
                    seasonGrid.positionViewAtIndex(seasonIndex, GridView.Contain)
                } else if (event.key === Qt.Key_Right) {
                    seasonIndex = Math.min(app.seasons.length - 1, seasonIndex + 1)
                    seasonGrid.positionViewAtIndex(seasonIndex, GridView.Contain)
                } else if (event.key === Qt.Key_Up) {
                    if (seasonIndex - scols >= 0) {
                        seasonIndex -= scols
                        seasonGrid.positionViewAtIndex(seasonIndex, GridView.Contain)
                    }
                } else if (event.key === Qt.Key_Down) {
                    if (seasonIndex + scols < app.seasons.length) {
                        seasonIndex += scols
                        seasonGrid.positionViewAtIndex(seasonIndex, GridView.Contain)
                    }
                } else if (event.key === Qt.Key_Y) {
                    if (app.seasons.length > 0 && seasonIndex >= 0 && seasonIndex < app.seasons.length)
                        app.toggleSeasonPlayed(app.seasons[seasonIndex].id)
                } else if (event.key === Qt.Key_Return) {
                    openSeasonNow()
                } else if (event.key === Qt.Key_Escape) {
                    goBack()
                } else handled = false
            } else if (page === "episodes") {
                if (root.seasonActionsFocus) {
                    if (event.key === Qt.Key_Left) { seasonAction = Math.max(0, seasonAction - 1) }
                    else if (event.key === Qt.Key_Right) { seasonAction = Math.min(2, seasonAction + 1) }
                    else if (event.key === Qt.Key_Down) { seasonActionsFocus = false }
                    else if (event.key === Qt.Key_Return) { seasonActionConfirm() }
                    else if (event.key === Qt.Key_Escape) { goBack() }
                    else handled = false
                } else {
                    if (event.key === Qt.Key_Up) {
                        if (epIndex === 0) {
                            seasonActionsFocus = true
                        } else {
                            epIndex = Math.max(0, epIndex - 1)
                            episodeList.positionViewAtIndex(epIndex, ListView.Contain)
                        }
                    } else if (event.key === Qt.Key_Down) {
                        if (app.episodes.length > 0) {
                            epIndex = Math.min(app.episodes.length - 1, epIndex + 1)
                            episodeList.positionViewAtIndex(epIndex, ListView.Contain)
                        }
                    } else if (event.key === Qt.Key_Return) {
                        if (app.episodes.length > 0) openDetail(app.episodes[epIndex].id)
                    } else if (event.key === Qt.Key_Escape) {
                        goBack()
                    } else handled = false
                }
            } else if (page === "library" || page === "favorites") {
                var cols = Math.max(1, Math.floor(libraryGrid.width / libraryGrid.cellWidth))
                if (event.key === Qt.Key_Left) { moveCol(-1) }
                else if (event.key === Qt.Key_Right) { moveCol(1) }
                else if (event.key === Qt.Key_Up) {
                    if (gridIndex < cols) {
                        navFocus = true
                        navIndex = (page === "favorites") ? 1 : 0
                    } else moveGridRow(-1)
                }
                else if (event.key === Qt.Key_Down) { moveGridRow(1) }
                else if (event.key === Qt.Key_Return) { activate() }
                else if (event.key === Qt.Key_Escape) { goBack() }
                else handled = false
            } else if (page === "settings" || page === "account") {
                var entries = (page === "settings") ? settingsMenu : accountMenu
                if (event.key === Qt.Key_Up) {
                    if (menuIndex === 0) {
                        var back = (page === "settings") ? 3 : 2
                        page = "home"
                        navFocus = true
                        navIndex = back
                    } else {
                        menuIndex = menuIndex - 1
                        menuList.positionViewAtIndex(menuIndex, ListView.Contain)
                    }
                } else if (event.key === Qt.Key_Down) {
                    menuIndex = Math.min(entries.length - 1, menuIndex + 1)
                    menuList.positionViewAtIndex(menuIndex, ListView.Contain)
                } else if (event.key === Qt.Key_Return) {
                    var entry = entries[menuIndex]
                    var action = entry.action
                    if (action === "logout" || action === "switchserver") {
                        app.logout()
                    } else if (action === "quality") {
                        app.cycleDefaultQuality(1)
                    } else if (action === "osd") {
                        app.cycleOsdSeconds(1)
                    } else if (action === "test") {
                        app.testConnection()
                    } else if (action === "language") {
                        root.languageIndex = root.currentLanguageIndex()
                        root.languageOpen = true
                    } else if (action === "reset") {
                        app.resetSettings()
                    } else if (action === "info") {
                        menuHint = page === "account"
                            ? (app.userName + "   ·   " + app.serverLabel)
                            : ("PolypLink " + appVersion)
                        menuHintTimer.restart()
                    } else {
                        menuHint = qsTr("Voce in preparazione: %1").arg(entry.title)
                        menuHintTimer.restart()
                    }
                } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Left) {
                    var back = (page === "settings") ? 3 : 2
                    page = "home"
                    navFocus = true
                    navIndex = back
                } else handled = false
            } else {
                if (event.key === Qt.Key_Left) { moveCol(-1) }
                else if (event.key === Qt.Key_Right) { moveCol(1) }
                else if (event.key === Qt.Key_Up) {
                    if (rowIndex <= firstRow()) { navFocus = true; navIndex = 0 }
                    else moveRow(-1)
                }
                else if (event.key === Qt.Key_Down) { moveRow(1) }
                else if (event.key === Qt.Key_Return) {
                    if (root.homeEmpty() && app.statusMessage.length > 0) app.refreshHome()
                    else activate()
                }
                else if (event.key === Qt.Key_Escape) { goBack() }
                else handled = false
            }

            event.accepted = handled
        }
    }
}
