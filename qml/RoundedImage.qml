import QtQuick 2.14
import QtGraphicalEffects 1.0

// Image with real rounded corners: Image cannot be clipped by a
// Rectangle with radius (Qt's clip is rectangular), so a
// mask is used. The cost is one framebuffer per image, acceptable for a few cards.
Item {
    id: ri

    property string source: ""
    property real radius: 8
    property int sourceSizeWidth: 340
    property int fillMode: Image.PreserveAspectCrop
    readonly property bool ready: img.status === Image.Ready

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
