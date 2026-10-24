import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// No project open: start one, or pick one with P.
Item {
    id: start

    function create() {
        if (backend.createProject(name.text, number.text)) {
            name.text = ""
            number.text = ""
        }
    }

    Pane {
        anchors.centerIn: parent
        width: 420
        padding: 28
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#17171a"; radius: theme.radius; border.color: "#2a2a30" }

        ColumnLayout {
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: 12

            Label {
                text: qsTr("Start a project")
                font.pixelSize: 20
                font.bold: true
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Joist keeps its RFIs and submittals, and who holds each one.")
                color: "#8a8a94"
                wrapMode: Text.Wrap
            }
            TextField {
                id: name
                Layout.fillWidth: true
                placeholderText: qsTr("Project name")
                focus: start.visible
                onAccepted: start.create()
            }
            TextField {
                id: number
                Layout.fillWidth: true
                placeholderText: qsTr("Project number (optional)")
                onAccepted: start.create()
            }
            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Open a project…")
                    flat: true
                    visible: backend.projects.length > 0
                    onClicked: projectPicker.open()
                }
                Item { Layout.fillWidth: true }
                AccentButton {
                    text: qsTr("Create")
                    enabled: name.text.trim() !== ""
                    onClicked: start.create()
                }
            }
        }
    }
}
