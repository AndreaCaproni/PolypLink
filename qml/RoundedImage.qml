import QtQuick 2.14
import QtGraphicalEffects 1.0

// Immagine con angoli arrotondati veri: Image non può essere ritagliata da un
// Rectangle con radius (il clip di Qt è rettangolare), quindi si usa una
// maschera. Il costo è un framebuffer per immagine, accettabile per poche card.
Item {
    id: ri

    property string source: ""
    property real radius: 8
    property int sourceSizeWidth: 340
    property int fillMode: Image.PreserveAspectCrop

    Image {
        id: img
        anchors.fill: parent
        source: ri.source
        asynchronous: true
        cache: true
        fillMode: ri.fillMode
        sourceSize.width: ri.sourceSizeWidth
        visible: false
        layer.enabled: true
        layer.smooth: true
    }

    Rectangle {
        id: mask
        anchors.fill: parent
        radius: ri.radius
        visible: false
        layer.enabled: true
    }

    OpacityMask {
        anchors.fill: parent
        source: img
        maskSource: mask
        visible: img.status === Image.Ready
    }
}
