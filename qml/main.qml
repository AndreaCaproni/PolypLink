import QtQuick 2.14
import QtQuick.Window 2.14
import "."

Window {
    id: root
    // The window stays visible: hiding it on this EGLFS device
    // turns off the display and the video plane is no longer composited.
    visible: true
    width: Screen.width > 0 ? Screen.width : Theme.designWidth
    height: Screen.height > 0 ? Screen.height : Theme.designHeight
    color: app.playerActive ? Theme.transparent : Theme.bg

    // --- navigation ----------------------------------------------------------
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
    // After "mark as watched" the replay button may disappear: focus must
    // follow the Watched button, not stay on the numeric index.
    property bool pendingPlayedFocus: false
    property int rowIndex: 0
    // top bar: reached with "up" from the first row
    property bool navFocus: false
    property int navIndex: 0
    // season page: focus on the buttons (true) or on the episodes (false)
    property bool seasonActionsFocus: false
    property string seasonAction: "play"
    readonly property var seasonActionIds: ["play", "watched", "favorite"]

    // Settings and Account side menus: entries planned for the stable
    // version, placeholders for now.
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

    // --- login ---------------------------------------------------------------
    property string authStep: "menu"     // menu | credentials
    property int authIndex: 0
    property string credUser: ""
    property string credPass: ""

    // --- keyboard ------------------------------------------------------------
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
                root.credPass = ""
                keyboard.text = ""
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

    // As in the official client: if there is a resume point, next to "Resume"
    // a "Watch from the beginning" option appears. Focus indices shift by one.
    function hasResume() {
        return app.detail.resumeTicks !== undefined && app.detail.resumeTicks > 0
    }

    function isSeries() {
        return app.detail.type === "Series"
    }

    // Episode: last button "Go to the full series".
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

    // For series: 0 Play, 1 Seasons, 2 Watched, 3 Favorite (no
    // playback settings). For movies/episodes the previous indices
    // remain, with the extra slot for the series when needed.
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

    function moveSeasonAction(delta) {
        var i = seasonActionIds.indexOf(seasonAction)
        if (i < 0) i = 0
        seasonAction = seasonActionIds[(i + delta + seasonActionIds.length) % seasonActionIds.length]
    }

    function seasonActionConfirm() {
        if (seasonAction === "play") app.playSeasonFromStart()
        else if (seasonAction === "watched") app.toggleSeasonPlayed(app.seasonInfo.id)
        else if (seasonAction === "favorite") app.toggleSeasonFavorite(app.seasonInfo.id)
    }

    function openSeasonNow() {
        if (app.seasons.length === 0) return
        root.epIndex = 0
        root.seasonActionsFocus = false
        root.seasonAction = "play"
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
                // no "account" page left hanging after a logout
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
            // Stop at the row edges: right must not wrap to the next row.
            var count = app.libraryItems.length
            if (count === 0) return
            var cols = Math.max(1, libraryPage.columns)
            var row = Math.floor(gridIndex / cols)
            var col = gridIndex % cols
            var nextCol = col + delta
            if (nextCol < 0 || nextCol >= cols) return
            var next = row * cols + nextCol
            if (next < 0 || next >= count) return
            gridIndex = next
            libraryPage.ensureVisible(gridIndex)
            return
        }
        var len = rowLength(rowIndex)
        if (len === 0) return
        colIndex = clamp(colIndex + delta, 0, len - 1)
        ensureRowVisible()
    }

    function moveGridRow(delta) {
        // the real columns: the ones the GridView is drawing
        var cols = libraryPage.columns
        var count = app.libraryItems.length
        if (count === 0) return

        var next = gridIndex + delta * cols
        if (next < 0) {                            // above the first row: nav bar
            navFocus = true
            navIndex = (page === "favorites") ? 1 : navLibraryIndex()
            return
        }
        if (next >= count) return                  // there is no row below

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
        if (keyboardOpen) {
            keyboardOpen = false
            if (kbTarget === "pass") {
                credPass = ""
                keyboard.text = ""
            }
            return
        }
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
        if (app.state === "auth") {
            if (authStep === "credentials") { authStep = "menu"; return }
            app.backToServerList()
            return
        }
        if (app.state === "quickconnect") { app.cancelQuickConnect(); return }
        Qt.quit()
    }

    // --- virtual canvas 1280x720, scaled to the real resolution ------------
    // The whole UI is drawn in 1280x720 coordinates. Here it is scaled to
    // fill the screen while keeping the aspect ratio: on a 1080p (or other)
    // display nothing breaks and nothing becomes microscopic.
    Item {
        id: canvas
        visible: !app.playerActive
        width: Theme.designWidth
        height: Theme.designHeight
        scale: Math.min(root.width / width, root.height / height)
        transformOrigin: Item.TopLeft
        x: Math.round((root.width - width * scale) / 2)
        y: Math.round((root.height - height * scale) / 2)

    // --------------------------------------------------------------- background
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
        insecureHttp: app.insecureHttp
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

    // ----------------------------------------------------- EMPTY HOME / OFFLINE
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

    // -------------------------------------------------------------------- LIBRARY
    LibraryPage {
        id: libraryPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "library" || page === "favorites")
        title: page === "favorites" ? qsTr("Preferiti") : app.libraryTitle
        items: app.libraryItems
        gridIndex: root.gridIndex
        navFocus: root.navFocus
        countText: root.countItems(app.libraryItems.length)
        emptyLabel: page === "favorites" ? qsTr("Nessun preferito")
                                          : qsTr("Nessuna voce in questa libreria")
        loading: app.loading
    }

    // ---------------------------------------------------------- SETTINGS / ACCOUNT
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

    // --------------------------------------------------------------------- SEASON
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

    // -------------------------------------------------------------------- SEASONS
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

    // -------------------------------------------------------------------- DETAIL
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

    // ------------------------------------------------------------------ keyboard
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
                        else if (root.kbTarget === "pass") {
                            root.credPass = value
                            keyboard.text = ""
                        }
                        root.keyboardOpen = false
                    }
                    onCancelled: {
                        root.keyboardOpen = false
                        if (root.kbTarget === "pass") {
                            root.credPass = ""
                            keyboard.text = ""
                        }
                    }
                }

            }
        }
    }

    // -------------------------------------------------------------------- status
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

    // ---------------------------------------------------------- FULL SYNOPSIS
    OverviewOverlay {
        id: overviewOverlay
        anchors.fill: parent
        visible: root.overviewOpen && root.detailOpen
        z: 50
        detail: app.detail
    }

    }
    // ---------------------------------------------- SETTINGS SELECTION
    SettingsChoiceOverlay {
        id: settingsChoiceOverlay
        anchors.fill: parent
        visible: root.settingsChoiceOpen
        z: 60
        title: root.settingsChoiceTitle
        action: root.settingsChoiceAction
        items: root.settingsChoiceItems
        index: root.settingsChoiceIndex
        audioDelay: root.settingsAudioDelay
    }

    // ------------------------------------------------------ INTERFACE LANGUAGE
    LanguageOverlay {
        id: languageOverlay
        anchors.fill: parent
        visible: root.languageOpen
        z: 60
        title: qsTr("Lingua dell'interfaccia")
        languages: language.available
        currentIndex: root.languageIndex
    }

    // ------------------------------------------------------------------ PLAYER
    PlayerPage {
        id: playerPage
        screenWidth: root.width
        screenHeight: root.height
        labels: ({
            audio: qsTr("Audio"),
            subtitles: qsTr("Sottotitoli"),
            quality: qsTr("Qualità"),
            audioDelay: qsTr("Ritardo audio"),
            stats: qsTr("Statistiche"),
            resume: qsTr("Riprendi"),
            nextEpisode: qsTr("Prossimo episodio"),
            play: qsTr("Riproduci"),
            transcoding: qsTr("Transcodifica (server)"),
            resolution: qsTr("Risoluzione   %1"),
            bitrate: qsTr("Bitrate       %1"),
            codec: qsTr("Codec         %1"),
            fps: qsTr("FPS inviati   %1"),
            queueVideo: qsTr("Coda video    %1 frame"),
            queueAudio: qsTr("Coda audio    %1 ms"),
            segment: qsTr("Segmento      %1 ms (download+demux)"),
            failed: qsTr("Frame rifiutati  %1"),
            dropped: qsTr("Scartati (ritardo)  %1")
        })
    }

    // -------------------------------------------------------------------- input
    FocusScope {
        anchors.fill: parent
        focus: true

        Keys.onPressed: {
            // With a USB keyboard the keys arrive here (the controller goes
            // through main.cpp): same OSD, same commands.
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

            // keyboard open: everything goes to it
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

            // top bar: left/right select, A opens, down returns to the content
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
                    settingsChoiceOverlay.ensureVisible(root.settingsChoiceIndex)
                } else if (event.key === Qt.Key_Down) {
                    root.settingsChoiceIndex = Math.min(root.settingsChoiceItems.length - 1,
                                                       root.settingsChoiceIndex + 1)
                    settingsChoiceOverlay.ensureVisible(root.settingsChoiceIndex)
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
                    overviewOverlay.scrollBy(120)
                } else if (event.key === Qt.Key_Up) {
                    overviewOverlay.scrollBy(-120)
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
                    if (event.key === Qt.Key_Left) moveSeasonAction(-1)
                    else if (event.key === Qt.Key_Right) moveSeasonAction(1)
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
