import QtQuick 2.14
import QtQuick.Window 2.14
import "."

Window {
    id: root
    // La finestra resta visibile: nascondendola, su questo dispositivo EGLFS
    // spegne il display e il piano video non viene piu' composto.
    visible: true
    width: Screen.width > 0 ? Screen.width : 1280
    height: Screen.height > 0 ? Screen.height : 720
    color: app.playerActive ? Theme.transparent : Theme.bg

    // --- navigazione ---------------------------------------------------------
    property string page: "home"
    property bool detailOpen: false
    property bool overviewOpen: false
    property bool languageOpen: false
    property int languageIndex: 0
    property bool settingsChoiceOpen: false
    property string settingsChoiceAction: ""
    property string settingsChoiceTitle: ""
    property var settingsChoiceItems: []
    property int settingsChoiceIndex: 0
    property int settingsAudioDelay: 0
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
        { title: qsTr("Bitrate massimo"), desc: qsTr("Bitrate predefinito: 4–100 Mbps"), action: "quality" },
        { title: qsTr("Ritardo audio"), desc: qsTr("Offset audio/video predefinito in millisecondi"), action: "audiodelay" },
        { title: qsTr("Sottotitoli"), desc: qsTr("Lingua predefinita, attivazione e burn-in") },
        { title: qsTr("Dissolvenza OSD"), desc: qsTr("Quanto resta visibile l'OSD"), action: "osd" },
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
        { title: qsTr("Sessione e server"), desc: qsTr("Stato della connessione e versione del server"), action: "info" },
        { title: qsTr("Righe della home"), desc: qsTr("Scegli quali righe mostrare") }
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
        homePage.ensureRowVisible(rowIndex)
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
        if (e.action === "audiodelay")
            return (app.audioDelayMs > 0 ? "+" : "") + app.audioDelayMs + " ms"
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

    function openSettingsChoice(action) {
        var items = []
        var current = 0
        settingsChoiceAction = action
        if (action === "quality") {
            var values = [4000, 8000, 12000, 20000, 30000, 40000, 50000, 60000, 80000, 100000]
            for (var i = 0; i < values.length; ++i) {
                var selected = values[i] === app.qualityDefaultKbps
                if (selected) current = i
                items.push({ label: (values[i] / 1000) + " Mbps", value: values[i], selected: selected })
            }
            settingsChoiceTitle = qsTr("Bitrate massimo")
        } else if (action === "osd") {
            var seconds = [3, 5, 8]
            for (var j = 0; j < seconds.length; ++j) {
                var sel = seconds[j] === app.osdSeconds
                if (sel) current = j
                items.push({ label: qsTr("%1 secondi").arg(seconds[j]), value: seconds[j], selected: sel })
            }
            settingsChoiceTitle = qsTr("Dissolvenza OSD")
        } else if (action === "audiodelay") {
            root.settingsAudioDelay = app.audioDelayMs
            settingsChoiceTitle = qsTr("Ritardo audio")
        }
        settingsChoiceItems = items
        settingsChoiceIndex = current
        settingsChoiceOpen = true
    }

    function confirmSettingsChoice() {
        if (settingsChoiceAction === "audiodelay") {
            app.setAudioDelay(settingsAudioDelay)
            settingsChoiceOpen = false
            return
        }
        var item = settingsChoiceItems[settingsChoiceIndex]
        if (item === undefined) return
        if (settingsChoiceAction === "quality") app.setDefaultQuality(item.value)
        else if (settingsChoiceAction === "osd") app.setOsdSeconds(item.value)
        settingsChoiceOpen = false
    }

    function currentMenuEntry() {
        var list = (page === "settings") ? settingsMenu : accountMenu
        if (menuIndex < 0 || menuIndex >= list.length)
            return { title: "", desc: "", action: "" }
        return list[menuIndex]
    }

    function isComingSoon() {
        var action = currentMenuEntry().action
        return action === undefined || action === ""
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

    function navAccountIndex() { return 2 + app.views.length }
    function navSettingsIndex() { return 3 + app.views.length }
    function navLibraryIndex() { return 2 + Math.max(0, app.libraryIndex) }

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
        } else if (navIndex < 2 + app.views.length) {
            app.openView(navIndex - 2)
            page = "library"
            gridIndex = 0
            navFocus = false
        } else if (navIndex === navAccountIndex()) {
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
            libraryPage.ensureVisible(gridIndex)
            return
        }
        var len = rowLength(rowIndex)
        if (len === 0) return
        colIndex = clamp(colIndex + delta, 0, len - 1)
        ensureRowVisible()
    }

    function moveGridRow(delta) {
        // le colonne reali: quelle che il GridView sta disegnando
        var cols = libraryPage.columns
        var count = app.libraryItems.length
        if (count === 0) return

        var next = gridIndex + delta * cols
        if (next < 0) {                            // sopra la prima riga: barra
            navFocus = true
            navIndex = (page === "favorites") ? 1 : navLibraryIndex()
            return
        }
        if (next >= count) return                  // non c'e' una riga sotto

        gridIndex = next
        libraryPage.ensureVisible(gridIndex)
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
            if (si >= app.servers.length) openKeyboard("url", "")
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
        if (settingsChoiceOpen) { settingsChoiceOpen = false; return }
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
            var back = (page === "settings") ? navSettingsIndex() : navAccountIndex()
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
            GradientStop { position: 0.0; color: Theme.bg }
            GradientStop { position: 1.0; color: Theme.bgAlt }
        }
    }

    // ------------------------------------------------------------------ header
    HeaderBar {
        id: header
        navFocus: root.navFocus
        navIndex: root.navIndex
        page: root.page
        appState: app.state
        serverLabel: app.serverLabel
        userName: app.userName
        libraries: app.views
        libraryIndex: app.libraryIndex
    }

    // ------------------------------------------------------------------- SERVER
    ServerPage {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "login"
        selectedIndex: root.serverIndex
        servers: app.servers
        searching: app.searching
        serverLabel: app.serverLabel
    }

    // --------------------------------------------------------------------- AUTH
    AuthPage {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "auth"
        serverLabel: app.serverLabel
        authStep: root.authStep
        authIndex: root.authIndex
        credUser: root.credUser
        credPass: root.credPass
    }

    // ------------------------------------------------------------ QUICK CONNECT
    QuickConnectPage {
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "quickconnect"
        code: app.quickConnectCode
        status: app.quickConnectStatus
    }

    // -------------------------------------------------------------------- HOME
    HomePage {
        id: homePage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && page === "home"
        rowIndex: root.rowIndex
        colIndex: root.colIndex
        navFocus: root.navFocus
        resumeItems: app.resume
        nextUpItems: app.nextUp
        latestItems: app.latest
        libraryViews: app.views
        resumeTitle: qsTr("Continua a guardare")
        nextUpTitle: qsTr("Prossimo episodio")
        latestTitle: qsTr("Aggiunti di recente")
        librariesTitle: qsTr("Librerie")
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
                color: Theme.dangerText
                font.pixelSize: 24
                font.bold: true
                wrapMode: Text.WordWrap
                text: app.statusMessage
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 36

                Row {
                    spacing: 9
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: Theme.transparent
                        border.width: 2
                        border.color: Theme.accentSoft
                        Text {
                            anchors.centerIn: parent
                            color: Theme.accentSoft
                            font.pixelSize: 15
                            font.bold: true
                            text: "A"
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.text
                        font.pixelSize: 17
                        text: qsTr("Riprova")
                    }
                }

                Row {
                    spacing: 9
                    Rectangle {
                        width: 28
                        height: 28
                        radius: 14
                        color: Theme.transparent
                        border.width: 2
                        border.color: Theme.accentSoft
                        Text {
                            anchors.centerIn: parent
                            color: Theme.accentSoft
                            font.pixelSize: 15
                            font.bold: true
                            text: "B"
                        }
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.text
                        font.pixelSize: 17
                        text: qsTr("Esci dall'app")
                    }
                }
            }
        }
    }

    // ----------------------------------------------------------------- LIBRERIA
    LibraryPage {
        id: libraryPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "library" || page === "favorites")
        title: app.libraryTitle
        items: app.libraryItems
        gridIndex: root.gridIndex
        navFocus: root.navFocus
        countText: root.countItems(app.libraryItems.length)
        emptyLabel: page === "favorites" ? qsTr("Nessun preferito")
                                          : qsTr("Nessuna voce in questa libreria")
        loading: app.loading
    }

    // -------------------------------------------------------- IMPOSTAZIONI / ACCOUNT
    MenuPage {
        id: menuPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "settings" || page === "account")
        title: page === "settings" ? qsTr("Impostazioni") : qsTr("Account")
        subtitle: page === "settings"
                  ? qsTr("Percorso verso la versione stabile")
                  : (app.userName + "   ·   " + app.serverLabel)
        entries: page === "settings" ? root.settingsMenu : root.accountMenu
        menuIndex: root.menuIndex
        accountMode: page === "account"
        profileView: root.isProfileView()
        profileName: app.userName
        profileImage: app.userImage
        serverLabel: app.serverLabel
        userStats: app.userStats
        currentTitle: root.currentMenuEntry().title
        currentValue: root.menuValue(root.currentMenuEntry())
        currentDesc: root.currentMenuEntry().desc
        comingSoon: root.isComingSoon()
        versionText: "PolypLink " + appVersion
        valueFor: function(entry) { return root.menuValue(entry) }
        moviesLabel: qsTr("Film")
        seriesLabel: qsTr("Serie")
        episodesLabel: qsTr("Episodi")
        favoritesLabel: qsTr("Preferiti")
        comingSoonLabel: qsTr("Funzionalità in arrivo")
    }

    // ------------------------------------------------------------------ STAGIONE
    EpisodesPage {
        id: episodesPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && root.page === "episodes" && !root.detailOpen
        seasonInfo: app.seasonInfo
        seasonTitle: app.seasonTitle
        seriesTitle: app.seriesTitle
        subtitleText: app.seriesTitle + "   ·   " + root.countEpisodes(app.episodes.length)
        episodes: app.episodes
        seriesPoster: app.seriesPoster
        seasonActionsFocus: root.seasonActionsFocus
        seasonAction: root.seasonAction
        epIndex: root.epIndex
        loading: app.loading
        watchFromStartLabel: qsTr("Guarda dall'inizio")
        watchedLabel: qsTr("Segna come visto")
        unwatchedLabel: qsTr("Segna come non visto")
        favoriteLabel: qsTr("Preferito")
        unfavoriteLabel: qsTr("Togli preferito")
        seenLabel: qsTr("VISTO")
        emptyLabel: qsTr("Nessun episodio in questa stagione")
    }

    // ----------------------------------------------------------------- STAGIONI
    SeasonsPage {
        id: seasonsPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && root.page === "seasons"
        seriesTitle: app.seriesTitle
        seasons: app.seasons
        seriesPoster: app.seriesPoster
        seasonIndex: root.seasonIndex
        countText: root.countSeasons(app.seasons.length)
        emptyLabel: qsTr("Nessuna stagione")
        episodeCountText: function(n) { return root.countEpisodes(n) }
        loading: app.loading
    }

    // ----------------------------------------------------------------- DETTAGLIO
    DetailPage {
        id: detailPage
        anchors.fill: parent
        visible: root.detailOpen
        z: 10
        detail: app.detail
        detailFocus: root.detailFocus

        isSeries: root.isSeries()
        hasResume: root.hasResume()
        hasSeriesLink: root.hasSeriesLink()
        isEpisode: root.isEpisode()
        hasOverview: root.hasOverview()

        playedIndex: root.playedIndex()
        favoriteIndex: root.favoriteIndex()
        seriesIndex: root.seriesIndex()
        previousIndex: root.previousIndex()
        readMoreIndex: root.readMoreIndex()
        settingsStart: root.settingsStart()

        qualityText: (app.qualityKbps / 1000) + " Mbps"
        audioText: root.audioLabel()
        subtitleText: root.subtitleLabel()
        metaText: root.metaLine(app.detail)

        playLabel: (root.hasResume() && !root.isSeries()) ? qsTr("Riprendi")
                                                          : qsTr("Riproduci")
        seasonsLabel: qsTr("Stagioni")
        restartLabel: qsTr("Riproduci dall'inizio")
        watchedLabel: qsTr("Segna come visto")
        unwatchedLabel: qsTr("Segna come non visto")
        favoriteLabel: qsTr("Preferito")
        unfavoriteLabel: qsTr("Togli preferito")
        seriesLabel: qsTr("Vai alla serie completa")
        previousLabel: qsTr("Episodio precedente")
        readMoreLabel: qsTr("Leggi tutto")
        qualityLabel: qsTr("Qualità")
        audioLabelText: qsTr("Audio")
        subtitleLabelText: qsTr("Sottotitoli")
    }

    // ------------------------------------------------------------------ tastiera
    Item {
        anchors.fill: parent
        visible: root.keyboardOpen
        z: 30

        Rectangle {
            anchors.fill: parent
            color: Theme.overlayDarker
        }

        Rectangle {
            anchors.centerIn: parent
            width: 1040
            height: 470
            radius: 14
            color: Theme.headerDeep
            border.width: 1
            border.color: Theme.borderStrong

            Column {
                anchors.centerIn: parent
                spacing: 14

                Rectangle {
                    width: 1000
                    height: 46
                    radius: 7
                    color: Theme.bgDeep
                    border.width: 1
                    border.color: Theme.borderStrong

                    Text {
                        anchors.left: parent.left
                        anchors.leftMargin: 14
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.text
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
        color: Theme.statusBar
        visible: app.statusMessage.length > 0 || app.loading

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 24
            anchors.verticalCenter: parent.verticalCenter
            color: Theme.textSubtle
            font.pixelSize: 15
            text: app.loading ? qsTr("caricamento…") : app.statusMessage
        }
    }


    // ------------------------------------------------------------------- toast
    Toast {
        text: app.notice
        bottomOffset: app.playerActive ? 260 : 44
    }

    // ---------------------------------------------------------- TRAMA COMPLETA
    Item {
        anchors.fill: parent
        visible: root.overviewOpen && root.detailOpen
        z: 50

        Rectangle {
            anchors.fill: parent
            color: Theme.overlay
        }

        Column {
            anchors.fill: parent
            anchors.margins: 60
            spacing: 16

            Text {
                width: parent.width
                color: Theme.text
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
                    color: Theme.textSoft
                    font.pixelSize: 18
                    lineHeight: 1.25
                    wrapMode: Text.WordWrap
                    text: app.detail.overview !== undefined ? app.detail.overview : ""
                }
            }
        }
    }

    }

    // ------------------------------------------- SELEZIONE IMPOSTAZIONI
    Item {
        anchors.fill: parent
        visible: root.settingsChoiceOpen
        z: 60

        Rectangle {
            anchors.fill: parent
            color: Theme.overlay
        }

        Column {
            anchors.centerIn: parent
            spacing: 14

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.text
                font.pixelSize: 28
                font.bold: true
                text: root.settingsChoiceTitle
            }

            Rectangle {
                visible: root.settingsChoiceAction !== "audiodelay"
                width: 520
                height: Math.min(460, 54 + root.settingsChoiceItems.length * 42)
                radius: 14
                color: Theme.overlayPanel
                border.width: 2
                border.color: Theme.accent

                ListView {
                    id: settingsChoiceList
                    anchors.fill: parent
                    anchors.margins: 10
                    clip: true
                    spacing: 2
                    model: root.settingsChoiceItems
                    boundsBehavior: Flickable.StopAtBounds

                    onVisibleChanged: {
                        if (visible)
                            positionViewAtIndex(root.settingsChoiceIndex, ListView.Contain)
                    }

                    delegate: Rectangle {
                        width: settingsChoiceList.width
                        height: 40
                        radius: 8
                        color: index === root.settingsChoiceIndex ? Theme.selected : "transparent"
                        border.width: index === root.settingsChoiceIndex ? 2 : 0
                        border.color: Theme.accent

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.right: parent.right
                            anchors.rightMargin: 28
                            anchors.verticalCenter: parent.verticalCenter
                            color: index === root.settingsChoiceIndex ? Theme.text : Theme.textSoft
                            font.pixelSize: 16
                            font.bold: index === root.settingsChoiceIndex
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
            }

            Rectangle {
                visible: root.settingsChoiceAction === "audiodelay"
                width: 520
                height: 110
                radius: 14
                color: Theme.overlayPanel
                border.width: 2
                border.color: Theme.accent

                Row {
                    anchors.centerIn: parent
                    spacing: 28

                    Text {
                        color: Theme.accentSoft
                        font.pixelSize: 30
                        font.bold: true
                        text: "<"
                    }

                    Text {
                        width: 160
                        horizontalAlignment: Text.AlignHCenter
                        color: Theme.text
                        font.pixelSize: 28
                        font.bold: true
                        text: (root.settingsAudioDelay > 0 ? "+" : "") +
                              root.settingsAudioDelay + " ms"
                    }

                    Text {
                        color: Theme.accentSoft
                        font.pixelSize: 30
                        font.bold: true
                        text: ">"
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
            color: Theme.overlay
        }

        Column {
            anchors.centerIn: parent
            spacing: 18

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.text
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
                        color: index === root.languageIndex ? Theme.selected : Theme.panel
                        border.width: index === root.languageIndex ? 2 : 1
                        border.color: index === root.languageIndex ? Theme.accent : Theme.border

                        Text {
                            anchors.centerIn: parent
                            color: index === root.languageIndex ? Theme.text : Theme.textSoft
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

                // fumetto con il tempo di destinazione
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
                PlayerButton { text: qsTr("Audio"); active: app.playerFocus === 4 }
                PlayerButton { text: qsTr("Sottotitoli"); active: app.playerFocus === 5 }
                PlayerButton { text: qsTr("Qualità"); active: app.playerFocus === 6 }
                PlayerButton { text: qsTr("Ritardo audio"); active: app.playerFocus === 7 }
                PlayerButton { text: qsTr("Statistiche"); active: app.playerFocus === 8 }
            }

        }

        // ---------------------------------------- MENU AUDIO/SOTTOTITOLI/QUALITA'
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

        // ------------------------------------------------ RITARDO AUDIO
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
                text: qsTr("Ritardo audio")
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
                        text: qsTr("Prossimo episodio")
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

            Rectangle { width: 16; height: 52; radius: 3; color: Theme.text }
            Rectangle { width: 16; height: 52; radius: 3; color: Theme.text }
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
                    text: qsTr("Transcodifica (server)")
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Risoluzione   %1").arg(app.playerStats.resolution !== undefined ? app.playerStats.resolution : "?")
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: app.playerStats.bandwidth !== undefined
                          ? qsTr("Bitrate       %1").arg((app.playerStats.bandwidth / 1000000).toFixed(1) + " Mbps")
                          : qsTr("Bitrate       %1").arg("?")
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Codec         %1").arg(app.playerStats.codecs !== undefined ? app.playerStats.codecs : "?")
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("FPS inviati   %1").arg(app.playerStats.fps !== undefined ? app.playerStats.fps : 0)
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Coda video    %1 frame").arg(app.playerStats.queueVideo !== undefined ? app.playerStats.queueVideo : 0)
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Coda audio    %1 ms").arg(app.playerStats.queueAudio !== undefined
                          ? Math.round(app.playerStats.queueAudio / 48) : 0)
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Segmento      %1 ms (download+demux)").arg(app.playerStats.segmentMs !== undefined ? app.playerStats.segmentMs : 0)
                }
                Text {
                    color: Theme.textSoft
                    font.pixelSize: 14
                    text: qsTr("Frame rifiutati  %1").arg(app.playerStats.failed !== undefined ? app.playerStats.failed : 0)
                }
                Text {
                    color: Theme.textSoft
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
            color: Theme.danger
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
                if (event.key === Qt.Key_Right) { root.navIndex = Math.min(navSettingsIndex(), root.navIndex + 1); event.accepted = true; return }
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

            if (root.settingsChoiceOpen) {
                if (root.settingsChoiceAction === "audiodelay") {
                    if (event.key === Qt.Key_Left) {
                        root.settingsAudioDelay = Math.max(-2000, root.settingsAudioDelay - 50)
                    } else if (event.key === Qt.Key_Right) {
                        root.settingsAudioDelay = Math.min(2000, root.settingsAudioDelay + 50)
                    } else if (event.key === Qt.Key_Return ||
                               event.key === Qt.Key_Escape ||
                               event.key === Qt.Key_Backspace) {
                        root.confirmSettingsChoice()
                    }
                    event.accepted = true
                    return
                }
                if (event.key === Qt.Key_Up) {
                    root.settingsChoiceIndex = Math.max(0, root.settingsChoiceIndex - 1)
                    settingsChoiceList.positionViewAtIndex(root.settingsChoiceIndex, ListView.Contain)
                } else if (event.key === Qt.Key_Down) {
                    root.settingsChoiceIndex = Math.min(root.settingsChoiceItems.length - 1,
                                                       root.settingsChoiceIndex + 1)
                    settingsChoiceList.positionViewAtIndex(root.settingsChoiceIndex, ListView.Contain)
                } else if (event.key === Qt.Key_Return) {
                    root.confirmSettingsChoice()
                } else if (event.key === Qt.Key_Escape ||
                           event.key === Qt.Key_Left ||
                           event.key === Qt.Key_Backspace) {
                    root.settingsChoiceOpen = false
                }
                event.accepted = true
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
                var scols = seasonsPage.columns
                if (event.key === Qt.Key_Left) {
                    seasonIndex = Math.max(0, seasonIndex - 1)
                    seasonsPage.ensureVisible(seasonIndex)
                } else if (event.key === Qt.Key_Right) {
                    seasonIndex = Math.min(app.seasons.length - 1, seasonIndex + 1)
                    seasonsPage.ensureVisible(seasonIndex)
                } else if (event.key === Qt.Key_Up) {
                    if (seasonIndex - scols >= 0) {
                        seasonIndex -= scols
                        seasonsPage.ensureVisible(seasonIndex)
                    }
                } else if (event.key === Qt.Key_Down) {
                    if (seasonIndex + scols < app.seasons.length) {
                        seasonIndex += scols
                        seasonsPage.ensureVisible(seasonIndex)
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
                            episodesPage.ensureVisible(epIndex)
                        }
                    } else if (event.key === Qt.Key_Down) {
                        if (app.episodes.length > 0) {
                            epIndex = Math.min(app.episodes.length - 1, epIndex + 1)
                            episodesPage.ensureVisible(epIndex)
                        }
                    } else if (event.key === Qt.Key_Return) {
                        if (app.episodes.length > 0) openDetail(app.episodes[epIndex].id)
                    } else if (event.key === Qt.Key_Escape) {
                        goBack()
                    } else handled = false
                }
            } else if (page === "library" || page === "favorites") {
                var cols = libraryPage.columns
                if (event.key === Qt.Key_Left) { moveCol(-1) }
                else if (event.key === Qt.Key_Right) { moveCol(1) }
                else if (event.key === Qt.Key_Up) {
                    if (gridIndex < cols) {
                        navFocus = true
                        navIndex = (page === "favorites") ? 1 : navLibraryIndex()
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
                        var back = (page === "settings") ? navSettingsIndex() : navAccountIndex()
                        page = "home"
                        navFocus = true
                        navIndex = back
                    } else {
                        menuIndex = menuIndex - 1
                        menuPage.ensureVisible(menuIndex)
                    }
                } else if (event.key === Qt.Key_Down) {
                    menuIndex = Math.min(entries.length - 1, menuIndex + 1)
                    menuPage.ensureVisible(menuIndex)
                } else if (event.key === Qt.Key_Return) {
                    var entry = entries[menuIndex]
                    var action = entry.action
                    if (action === "logout" || action === "switchserver") {
                        app.logout()
                    } else if (action === "quality" || action === "osd" || action === "audiodelay") {
                        root.openSettingsChoice(action)
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
                        menuHint = qsTr("Funzionalità in arrivo")
                        menuHintTimer.restart()
                    }
                } else if (event.key === Qt.Key_Escape || event.key === Qt.Key_Left) {
                    var back = (page === "settings") ? navSettingsIndex() : navAccountIndex()
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
