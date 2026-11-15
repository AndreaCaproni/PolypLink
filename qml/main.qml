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
        { title: "Server e connessione", desc: "Indirizzo, porta, HTTPS e riconnessione automatica" },
        { title: "Qualità massima", desc: "Bitrate predefinito: 4, 8, 12, 20 Mbps o automatico" },
        { title: "Risoluzione massima", desc: "720p, 1080p o automatica" },
        { title: "Audio preferito", desc: "Lingua e canali (stereo / 5.1)" },
        { title: "Sottotitoli", desc: "Lingua predefinita, attivazione e burn-in" },
        { title: "Precaricamento", desc: "Quanti segmenti tenere pronti prima di partire" },
        { title: "Seek e accelerazione", desc: "Passo iniziale e tetto dell'accelerazione" },
        { title: "OSD", desc: "Durata, contenuti e comportamento in pausa" },
        { title: "Lingua dell'interfaccia", desc: "Italiano, English" },
        { title: "Scala dell'interfaccia", desc: "Dimensione di testi e card" },
        { title: "Tema", desc: "Scuro, chiaro, automatico" },
        { title: "Rete e certificati", desc: "Proxy e certificati HTTPS non firmati" },
        { title: "Cache immagini", desc: "Dimensione massima e pulizia" },
        { title: "Diagnostica e log", desc: "Livello dei log ed esportazione su USB" },
        { title: "Aggiornamenti", desc: "Canale beta e controllo manuale" },
        { title: "Ripristino", desc: "Riporta le impostazioni ai valori predefiniti" },
        { title: "Informazioni", desc: "Versione, build, licenze e componenti" }
    ]

    property var accountMenu: [
        { title: "Profilo", desc: "Nome utente, avatar e server collegato" },
        { title: "Cambia utente", desc: "Torna alla schermata di accesso", action: "logout" },
        { title: "Esci", desc: "Chiude la sessione e cancella il token", action: "logout" },
        { title: "Quick Connect", desc: "Associa l'app senza digitare la password" },
        { title: "Sessione e server", desc: "Stato della connessione e versione del server" },
        { title: "Ripresa", desc: "Sincronizza la posizione di visione" },
        { title: "Righe della home", desc: "Scegli quali righe mostrare" },
        { title: "Prossimo episodio", desc: "Avvio automatico a fine puntata" },
        { title: "Statistiche di visione", desc: "Tempo visto e cronologia" }
    ]

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
        if (app.subtitleIndex < 0) return "Spenti"
        return root.trackLabel(app.subtitleTracks, app.subtitleIndex)
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

    // Per le serie: 0 Riproduci, 1 Stagioni, 2 Visto, 3 Preferito (niente
    // impostazioni di riproduzione). Per i film restano gli indici di prima.
    function settingsStart() {
        return isSeries() ? -1 : (hasResume() ? 4 : 3)
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
        else if (root.detailFocus === playedIndex()) app.togglePlayed()
        else if (root.detailFocus === favoriteIndex()) app.toggleFavorite()
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

        // logo Jellyfin + nome dell'app
        Row {
            x: 24
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Image {
                width: 34
                height: 34
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/qml/assets/jellyfin-logo.png"
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
                    text: "Jelly"
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
                text: "Home"
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
                text: "Preferiti"
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
            text: app.state === "login" ? "scegli il server" : app.serverLabel
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
                text: "Scegli il tuo server Jellyfin"
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#7f8c9b"
                font.pixelSize: 17
                wrapMode: Text.WordWrap
                text: app.searching
                      ? "Ricerca sulla rete locale…"
                      : (app.servers.length === 0
                         ? "Nessun server rilevato automaticamente."
                         : "Premi A per connetterti.")
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
                    Text { color: "#ffffff"; font.pixelSize: 20; text: "Server salvato" }
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
                    text: "Inserisci l'indirizzo del server…"
                }
            }

            Text {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                color: "#4a5a68"
                font.pixelSize: 15
                wrapMode: Text.WordWrap
                text: "A: seleziona    X/R: cerca in rete (LAN + UDP)    B: esci\n"
                      + "L'indirizzo si può anche mettere in\n"
                      + "steamlink/config/system/jellyfin-server.txt sulla chiavetta"
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
                text: "Accedi a " + app.serverLabel
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
                        text: "Nome utente e password"
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
                        text: "Quick Connect (codice a 6 cifre)"
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
                        text: "nome utente"
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
                        text: "password"
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
                        text: "Entra"
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
                text: "Collega questo dispositivo"
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
                title: "Continua a guardare"
                items: app.resume
                landscape: true
                active: root.rowIndex === 0 && !root.navFocus
                currentIndex: root.colIndex
                visible: app.resume.length > 0
            }

            MediaRow {
                id: rowNextUp
                width: parent.width
                title: "Prossimo episodio"
                items: app.nextUp
                landscape: true
                active: root.rowIndex === 1 && !root.navFocus
                currentIndex: root.colIndex
                visible: app.nextUp.length > 0
            }

            MediaRow {
                id: rowLatest
                width: parent.width
                title: "Aggiunti di recente"
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
                    text: "Librerie"
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
            text: app.libraryItems.length + " elementi"
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
        text: page === "favorites" ? "Nessun preferito" : "Nessuna voce in questa libreria"
    }

    // -------------------------------------------------------- IMPOSTAZIONI / ACCOUNT
    Item {
        id: menuPage
        anchors.fill: parent
        anchors.topMargin: header.height
        visible: app.state === "home" && (page === "settings" || page === "account")

        // il pannello entra da destra quando si apre la pagina
        onVisibleChanged: {
            menuPanel.panelShift = visible ? 0 : menuPanel.width
            if (visible) menuList.positionViewAtIndex(0, ListView.Beginning)
        }

        // lato sinistro: titolo della sezione
        Text {
            x: 48
            y: 170
            color: "#ffffff"
            font.pixelSize: 44
            font.bold: true
            text: page === "settings" ? "IMPOSTAZIONI" : "ACCOUNT"
        }
        Text {
            x: 48
            y: 236
            width: 540
            color: "#6f8090"
            font.pixelSize: 16
            wrapMode: Text.WordWrap
            text: page === "settings"
                  ? "Connessione, qualità, audio, interfaccia e diagnostica del client."
                  : "Profilo, sessione e preferenze legate all'utente."
        }
        Text {
            x: 48
            y: 292
            width: 520
            color: "#4a5a68"
            font.pixelSize: 14
            wrapMode: Text.WordWrap
            text: "L'elenco a destra è quello che manca per la versione stabile: per ora le voci sono segnaposto."
        }
        Text {
            x: 48
            y: 354
            color: "#3f4c58"
            font.pixelSize: 13
            text: "JellyLink " + appVersion
        }

        // menu a scomparsa dal lato destro
        Rectangle {
            id: menuPanel
            width: 640
            height: parent.height
            y: 0
            x: parent.width - width + panelShift
            color: "#0e1620"
            border.width: 1
            border.color: "#1c2a38"
            Behavior on x { NumberAnimation { duration: 220; easing.type: Easing.OutCubic } }

            property real panelShift: width

            Column {
                anchors.fill: parent
                anchors.leftMargin: 22
                anchors.rightMargin: 22
                anchors.topMargin: 16
                anchors.bottomMargin: 16
                spacing: 8

                Text {
                    color: "#7fd9ff"
                    font.pixelSize: 20
                    font.bold: true
                    text: page === "settings" ? "Voci delle impostazioni" : "Voci dell'account"
                }
                Text {
                    color: "#6f8090"
                    font.pixelSize: 13
                    text: "Percorso verso la versione stabile"
                }

                ListView {
                    id: menuList
                    width: parent.width
                    height: parent.height - 88
                    clip: true
                    model: page === "settings" ? root.settingsMenu : root.accountMenu
                    spacing: 4
                    boundsBehavior: Flickable.StopAtBounds

                    delegate: Rectangle {
                        width: menuList.width
                        height: 58
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
                            anchors.rightMargin: 14
                            anchors.top: parent.top
                            anchors.topMargin: 8
                            color: index === root.menuIndex ? "#ffffff" : "#dbe5ef"
                            font.pixelSize: 16
                            font.bold: index === root.menuIndex
                            elide: Text.ElideRight
                            text: modelData.title
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.right: parent.right
                            anchors.rightMargin: 14
                            anchors.bottom: parent.bottom
                            anchors.bottomMargin: 8
                            color: "#7f8c9b"
                            font.pixelSize: 13
                            elide: Text.ElideRight
                            text: modelData.desc
                        }
                    }
                }

                Text {
                    visible: root.menuHint.length > 0
                    color: "#7fd9ff"
                    font.pixelSize: 13
                    text: root.menuHint
                }
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
                    text: app.seriesTitle + "   ·   " + app.episodes.length + " episodi"
                }

                Row {
                    spacing: 10

                    Rectangle {
                        width: 230; height: 44; radius: 22
                        color: root.seasonActionsFocus && root.seasonAction === 0 ? "#1d5a75" : "#00a4dc"
                        border.width: root.seasonActionsFocus && root.seasonAction === 0 ? 2 : 1
                        border.color: "#7fd9ff"
                        Text { anchors.centerIn: parent
                               color: root.seasonActionsFocus && root.seasonAction === 0 ? "#ffffff" : "#00121a"
                               font.pixelSize: 15; font.bold: true
                               text: "Guarda dall'inizio" }
                    }

                    Rectangle {
                        width: 215; height: 44; radius: 22
                        color: root.seasonActionsFocus && root.seasonAction === 1 ? "#123243" : "#101920"
                        border.width: root.seasonActionsFocus && root.seasonAction === 1 ? 2 : 1
                        border.color: root.seasonActionsFocus && root.seasonAction === 1 ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: (app.seasonInfo.played !== undefined && app.seasonInfo.played)
                                     ? "Segna come non visto" : "Segna come visto" }
                    }

                    Rectangle {
                        width: 190; height: 44; radius: 22
                        color: root.seasonActionsFocus && root.seasonAction === 2 ? "#123243" : "#101920"
                        border.width: root.seasonActionsFocus && root.seasonAction === 2 ? 2 : 1
                        border.color: root.seasonActionsFocus && root.seasonAction === 2 ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: (app.seasonInfo.favorite !== undefined && app.seasonInfo.favorite)
                                     ? "Togli preferito" : "Preferito" }
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
                    Text { anchors.centerIn: parent; color: "#7fd9ff"; font.pixelSize: 12; text: "VISTO" }
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
        text: "Nessun episodio in questa stagione"
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
            text: app.seasons.length + " stagioni"
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
                    subtitle: (modelData.childCount !== undefined ? modelData.childCount : 0) + " episodi"
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
        text: "Nessuna stagione"
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

                // azioni affiancate, come nel client Jellyfin
                Row {
                    spacing: 10

                    Rectangle {
                        width: 190; height: 46; radius: 23
                        color: root.detailFocus === 0 ? "#00a4dc" : "#123243"
                        border.width: root.detailFocus === 0 ? 2 : 1
                        border.color: root.detailFocus === 0 ? "#7fd9ff" : "#1e2b38"
                        Text { anchors.centerIn: parent
                               color: root.detailFocus === 0 ? "#00121a" : "#ffffff"
                               font.pixelSize: 16; font.bold: true
                               text: (root.hasResume() && !root.isSeries())
                                     ? "Riprendi" : "Riproduci" }
                    }

                    // serie: si apre la pagina delle stagioni
                    Rectangle {
                        width: 150; height: 46; radius: 23
                        visible: root.isSeries()
                        color: root.detailFocus === 1 ? "#123243" : "#101920"
                        border.width: root.detailFocus === 1 ? 2 : 1
                        border.color: root.detailFocus === 1 ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: "Stagioni" }
                    }

                    // "Play from beginning" del client web: stesso posto, solo
                    // quando c'e' una posizione da cui riprendere.
                    Rectangle {
                        width: 230; height: 46; radius: 23
                        visible: root.hasResume() && !root.isSeries()
                        color: root.detailFocus === 1 ? "#123243" : "#101920"
                        border.width: root.detailFocus === 1 ? 2 : 1
                        border.color: root.detailFocus === 1 ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: "Riproduci dall'inizio" }
                    }

                    Rectangle {
                        width: 205; height: 46; radius: 23
                        color: root.detailFocus === root.playedIndex() ? "#123243" : "#101920"
                        border.width: root.detailFocus === root.playedIndex() ? 2 : 1
                        border.color: root.detailFocus === root.playedIndex() ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: (app.detail.played !== undefined && app.detail.played)
                                     ? "Segna come non visto" : "Segna come visto" }
                    }

                    Rectangle {
                        width: 190; height: 46; radius: 23
                        color: root.detailFocus === root.favoriteIndex() ? "#123243" : "#101920"
                        border.width: root.detailFocus === root.favoriteIndex() ? 2 : 1
                        border.color: root.detailFocus === root.favoriteIndex() ? "#00a4dc" : "#1b2733"
                        Text { anchors.centerIn: parent; color: "#ffffff"; font.pixelSize: 14
                               text: (app.detail.favorite !== undefined && app.detail.favorite)
                                     ? "Togli preferito" : "Preferito" }
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
                               color: "#9fb0c0"; font.pixelSize: 15; text: "Qualità" }
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
                               color: "#9fb0c0"; font.pixelSize: 15; text: "Audio" }
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
                               color: "#9fb0c0"; font.pixelSize: 15; text: "Sottotitoli" }
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
            text: app.loading ? "caricamento…" : app.statusMessage
        }
    }


    // ------------------------------------------------------------------- toast
    Rectangle {
        id: toast
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 44
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
                PlayerButton { text: "Audio"; active: app.playerFocus === 4 }
                PlayerButton { text: "Sottotitoli"; active: app.playerFocus === 5 }
                PlayerButton { text: "Qualità"; active: app.playerFocus === 6 }
                PlayerButton { text: "Statistiche"; active: app.playerFocus === 7 }
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
                    text: "Transcodifica (server)"
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Risoluzione   " + (app.playerStats.resolution !== undefined ? app.playerStats.resolution : "?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Bitrate       " + (app.playerStats.bandwidth !== undefined
                          ? (app.playerStats.bandwidth / 1000000).toFixed(1) + " Mbps (richiesti 12)" : "?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Codec         " + (app.playerStats.codecs !== undefined ? app.playerStats.codecs : "?")
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "FPS inviati   " + (app.playerStats.fps !== undefined ? app.playerStats.fps : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Coda video    " + (app.playerStats.queueVideo !== undefined ? app.playerStats.queueVideo : 0) + " frame"
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Coda audio    " + (app.playerStats.queueAudio !== undefined
                          ? Math.round(app.playerStats.queueAudio / 48) : 0) + " ms"
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Segmento      " + (app.playerStats.segmentMs !== undefined ? app.playerStats.segmentMs : 0) + " ms (download+demux)"
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Frame rifiutati  " + (app.playerStats.failed !== undefined ? app.playerStats.failed : 0)
                }
                Text {
                    color: "#c9d4df"
                    font.pixelSize: 14
                    text: "Scartati (ritardo)  " + (app.playerStats.dropped !== undefined ? app.playerStats.dropped : 0)
                }
            }
        }

        // impostazioni durante la riproduzione (Y)
        Rectangle {
            visible: app.settingsOpen
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 180
            width: 640
            height: 210
            radius: 12
            color: "#ee0b0f14"
            border.width: 1
            border.color: "#2b6f8c"

            Column {
                anchors.centerIn: parent
                spacing: 8

                Text {
                    color: "#7fd9ff"
                    font.pixelSize: 18
                    font.bold: true
                    text: "Impostazioni"
                }

                Rectangle {
                    width: 580; height: 38; radius: 7
                    color: app.settingsRow === 0 ? "#123243" : "#101920"
                    border.width: 1
                    border.color: app.settingsRow === 0 ? "#00a4dc" : "#1b2733"
                    Text { anchors.left: parent.left; anchors.leftMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#9fb0c0"; font.pixelSize: 16; text: "Qualità" }
                    Text { anchors.right: parent.right; anchors.rightMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#ffffff"; font.pixelSize: 16
                           text: (app.qualityKbps / 1000) + " Mbps" }
                }

                Rectangle {
                    width: 580; height: 38; radius: 7
                    color: app.settingsRow === 1 ? "#123243" : "#101920"
                    border.width: 1
                    border.color: app.settingsRow === 1 ? "#00a4dc" : "#1b2733"
                    Text { anchors.left: parent.left; anchors.leftMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#9fb0c0"; font.pixelSize: 16; text: "Audio" }
                    Text { anchors.right: parent.right; anchors.rightMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#ffffff"; font.pixelSize: 16
                           text: root.audioLabel() }
                }

                Rectangle {
                    width: 580; height: 38; radius: 7
                    color: app.settingsRow === 2 ? "#123243" : "#101920"
                    border.width: 1
                    border.color: app.settingsRow === 2 ? "#00a4dc" : "#1b2733"
                    Text { anchors.left: parent.left; anchors.leftMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#9fb0c0"; font.pixelSize: 16; text: "Sottotitoli" }
                    Text { anchors.right: parent.right; anchors.rightMargin: 12
                           anchors.verticalCenter: parent.verticalCenter
                           color: "#ffffff"; font.pixelSize: 16
                           text: root.subtitleLabel() }
                }

            }
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 40
            visible: app.playerStatus.length > 0
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
                var lastAction = (s >= 0) ? s - 1 : 3
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
                    menuIndex = Math.max(0, menuIndex - 1)
                    menuList.positionViewAtIndex(menuIndex, ListView.Contain)
                } else if (event.key === Qt.Key_Down) {
                    menuIndex = Math.min(entries.length - 1, menuIndex + 1)
                    menuList.positionViewAtIndex(menuIndex, ListView.Contain)
                } else if (event.key === Qt.Key_Return) {
                    var entry = entries[menuIndex]
                    if (entry.action === "logout") {
                        app.logout()
                    } else {
                        menuHint = "Voce in preparazione: " + entry.title
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
