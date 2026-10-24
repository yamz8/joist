import QtQuick
import QtQuick.Controls

// The main action, filled with the Omarchy accent.
Button {
    id: button
    implicitHeight: 38
    leftPadding: 18
    rightPadding: 18

    contentItem: Label {
        text: button.text
        font.bold: true
        color: theme.accentForeground
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
    }

    background: Rectangle {
        radius: Math.min(6, theme.radius)
        color: theme.accent
        opacity: button.enabled ? (button.down ? 0.8 : 1) : 0.35
        border.color: button.visualFocus ? "white" : "transparent"
    }
}
