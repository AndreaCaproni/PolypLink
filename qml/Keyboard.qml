import QtQuick 2.14

// Tastiera a schermo: serve perche' su una console non c'e' tastiera fisica.
// Include lettere (con maiuscole), cifre e i simboli che servono per un URL o
// una password. Layout ridotto ma completo per l'uso reale.
Item {
    id: kb

    property string text: ""
    property int row: 0
    property int col: 0
    property bool shift: false

    signal accepted(string value)
    signal cancelled()

    readonly property var rows: [
        ["1", "2", "3", "4", "5", "6", "7", "8", "9", "0"],
        ["q", "w", "e", "r", "t", "y", "u", "i", "o", "p"],
        ["a", "s", "d", "f", "g", "h", "j", "k", "l", "."],
        ["z", "x", "c", "v", "b", "n", "m", ":", "/", "-"],
        ["!", "@", "#", "$", "%", "&", "*", "(", ")", "_"],
        ["+", "=", ",", ";", "'", "\"", "[", "]", "?"],
        ["MAIUSC", "spazio", "CANCELLA", "ANNULLA", "OK"]
    ]

    width: 1000
    height: 360

    function rowLen(r) { return rows[r].length }

    function move(dx, dy) {
        if (dy !== 0) {
            row = Math.max(0, Math.min(rows.length - 1, row + dy))
            col = Math.min(col, rowLen(row) - 1)
        }
        if (dx !== 0) {
            col = Math.max(0, Math.min(rowLen(row) - 1, col + dx))
        }
    }

    function appendChar(c) {
        if (c.length === 1) text += c
    }

    function backspace() {
        text = text.substring(0, Math.max(0, text.length - 1))
    }

    function display(k) {
        if (shift && k.length === 1 && k >= "a" && k <= "z") return k.toUpperCase()
        if (k === "MAIUSC") return qsTr("MAIUSC")
        if (k === "spazio") return qsTr("spazio")
        if (k === "CANCELLA") return qsTr("CANCELLA")
        if (k === "ANNULLA") return qsTr("ANNULLA")
        return k
    }

    function press() {
        var k = rows[row][col]
        if (k === "OK") { accepted(text); return }
        if (k === "ANNULLA") { cancelled(); return }
        if (k === "CANCELLA") { backspace(); return }
        if (k === "spazio") { text += " "; return }
        if (k === "MAIUSC") { shift = !shift; return }
        if (shift && k.length === 1) text += k.toUpperCase()
        else text += k
    }

    Column {
        anchors.centerIn: parent
        spacing: 6

        Repeater {
            model: kb.rows

            Row {
                id: keyRow
                property int r: index
                spacing: 7

                Repeater {
                    model: kb.rows[r]

                    Rectangle {
                        width: ("" + modelData).length > 2 ? 186 : 90
                        height: 40
                        radius: 6
                        color: (kb.row === keyRow.r && kb.col === index) ? "#00a4dc" : "#18232e"
                        border.width: 1
                        border.color: (kb.row === keyRow.r && kb.col === index)
                                      ? "#7fd9ff" : "#243040"

                        Text {
                            anchors.centerIn: parent
                            color: (kb.row === keyRow.r && kb.col === index)
                                   ? "#00121a" : "#e6eef6"
                            font.pixelSize: 18
                            font.bold: (kb.row === keyRow.r && kb.col === index)
                            text: kb.display("" + modelData)
                        }
                    }
                }
            }
        }
    }
}
