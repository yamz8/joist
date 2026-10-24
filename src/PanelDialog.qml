import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

// Every dialog's frame: a dark panel on a dimmed window, drawn plainly so it
// looks the same whatever the renderer.
Dialog {
    id: dialog
    anchors.centerIn: Overlay.overlay
    modal: true
    padding: 20
    topPadding: 8
    Material.roundedScale: theme.radius

    background: Rectangle {
        color: "#17171a"
        radius: theme.radius
        border.color: "#2a2a30"
    }

    header: Label {
        text: dialog.title
        visible: text !== ""
        font.pixelSize: 18
        font.bold: true
        elide: Text.ElideRight
        padding: 20
        bottomPadding: 8
    }

    Overlay.modal: Rectangle { color: "#a0050506" }
}
