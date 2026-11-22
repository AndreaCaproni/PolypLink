pragma Singleton
import QtQuick 2.14

// Shared palette and metrics. A single place to change colors and spacing.
QtObject {
    // Background
    readonly property color bg: "#0b0f14"
    readonly property color bgAlt: "#101922"
    readonly property color bgDeep: "#0a1118"
    readonly property color header: "#111a23"
    readonly property color headerDeep: "#0f1821"
    readonly property color panel: "#141e28"
    readonly property color panelAlt: "#101920"
    readonly property color panelDeep: "#0e1620"
    readonly property color panelAlternate: "#121c26"
    readonly property color statusBar: "#0d151d"

    // Selection and borders
    readonly property color selected: "#123243"
    readonly property color chip: "#182430"
    readonly property color border: "#1e2b38"
    readonly property color borderSoft: "#1c2a38"
    readonly property color borderDark: "#1b2733"
    readonly property color borderStrong: "#243040"
    readonly property color borderAccent: "#2b6f8c"
    readonly property color borderDeep: "#1a2430"
    readonly property color primaryActive: "#1d5a75"
    readonly property color key: "#18232e"

    // Text
    readonly property color text: "#ffffff"
    readonly property color textSoft: "#dbe5ef"
    readonly property color textMuted: "#9fb0c0"
    readonly property color textDim: "#6f8090"
    readonly property color textSubtle: "#8fa3b6"
    readonly property color textFaint: "#5b6b7c"
    readonly property color textSubtleAlt: "#5f7c8f"
    readonly property color accentText: "#00121a"

    // Accent and states
    readonly property color accent: "#00a4dc"
    readonly property color accentSoft: "#7fd9ff"
    readonly property color accentGlow: "#2200a4dc"
    readonly property color accentGlowSoft: "#2e00a4dc"
    readonly property color danger: "#ff6b6b"
    readonly property color dangerSoft: "#ff8a8a"
    readonly property color dangerText: "#ffb3b3"
    readonly property color dangerBg: "#2a1216"
    readonly property color favorite: "#ff4d4d"
    readonly property color track: "#55ffffff"
    readonly property color transparent: "#00000000"

    // Overlays
    readonly property color overlay: "#f20b0f14"
    readonly property color overlayLight: "#cc0b0f14"
    readonly property color overlayPanel: "#ee0b0f14"
    readonly property color overlayMenu: "#990b0f14"
    readonly property color overlayDark: "#e6000000"
    readonly property color overlayDarker: "#c0000000"
    readonly property color overlayMedium: "#66000000"
    readonly property color posterOverlay: "#dd0b0f14"
    readonly property color toastBg: "#e6123243"
    readonly property color toastText: "#e6f7ff"

    // Metrics
    readonly property int designWidth: 1280
    readonly property int designHeight: 720
    readonly property int headerHeight: 56
    readonly property int radiusSmall: 8
    readonly property int radius: 12
    readonly property int radiusLarge: 16
    readonly property int spacingSmall: 6
    readonly property int spacing: 10
    readonly property int spacingLarge: 18
}
