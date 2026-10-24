import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A view in the bar, with how many open items it has and how many are late.
AbstractButton {
    id: tab
    property string label
    property int count
    property int late
    property bool active

    implicitHeight: 52
    implicitWidth: row.implicitWidth + 20

    contentItem: Item {
        RowLayout {
            id: row
            anchors.centerIn: parent
            spacing: 6

            Label {
                text: tab.label
                font.pixelSize: 14
                font.bold: tab.active
                color: tab.active ? "#f2f2f5" : (tab.hovered ? "#d8d8de" : "#8a8a94")
            }
            Label {
                text: tab.count.toLocaleString(Qt.locale(), 'f', 0)
                font.pixelSize: 12
                color: "#8a8a94"
                visible: tab.count > 0
            }
            Rectangle {
                visible: tab.late > 0
                implicitWidth: lateLabel.implicitWidth + 10
                implicitHeight: 18
                radius: 9
                color: "#5a1d22"
                Label {
                    id: lateLabel
                    anchors.centerIn: parent
                    text: tab.late.toLocaleString(Qt.locale(), 'f', 0)
                    font.pixelSize: 11
                    font.bold: true
                    color: "#ff9a9a"
                }
            }
        }
    }

    background: Item {
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            width: parent.width - 12
            height: 2
            color: theme.accent
            visible: tab.active
        }
    }
}
