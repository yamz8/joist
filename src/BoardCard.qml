import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// One open item on the board: what it is and how long they have.
AbstractButton {
    id: card
    property var item
    property bool selected

    implicitHeight: content.implicitHeight + 24
    leftPadding: 14
    rightPadding: 12
    hoverEnabled: true
    onPressed: win.leaveField()

    background: Rectangle {
        radius: Math.min(6, theme.radius)
        color: card.selected ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.12)
                             : card.hovered ? "#1d1d22" : "#18181c"
        border.color: card.selected ? theme.accent : "#26262c"
        border.width: card.selected ? 1.5 : 1

        // A red edge on anything overdue.
        Rectangle {
            visible: card.item.overdue
            width: 3
            radius: 1
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.margins: 1
            color: "#ff6b6b"
        }
    }

    contentItem: Item {
        ColumnLayout {
            id: content
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    text: card.item.code
                    font.family: "monospace"
                    font.pixelSize: 12
                    color: card.selected ? theme.accent : "#9a9aa4"
                }
                Label {
                    text: card.item.type === "submittal" && card.item.revision > 0
                        ? qsTr("Rev. %1").arg(card.item.revision) : ""
                    visible: text !== ""
                    font.pixelSize: 11
                    color: "#8a8a94"
                }
                Item { Layout.fillWidth: true }
                Label {
                    text: card.item.spec
                    visible: text !== ""
                    font.pixelSize: 11
                    color: "#6c6c76"
                }
            }

            Label {
                Layout.fillWidth: true
                text: card.item.title || qsTr("Untitled")
                color: card.item.title ? "#ececf0" : "#6c6c76"
                font.pixelSize: 14
                wrapMode: Text.Wrap
                maximumLineCount: 2
                elide: Text.ElideRight
            }

            Label {
                text: win.dueText(card.item)
                color: win.dueColor(card.item)
                font.pixelSize: 12
                font.bold: card.item.overdue
            }
        }
    }
}
