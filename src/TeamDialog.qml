import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The project's name and team. The first party raises new items and the
// second answers them; renaming a party carries through every item.
PanelDialog {
    id: dialog
    width: 480
    title: qsTr("Project and team")

    onOpened: {
        name.text = backend.projectName
        number.text = backend.projectNumber
        added.text = ""
        added.forceActiveFocus()
    }
    onClosed: win.leaveField()

    function saveProject() {
        if (name.text.trim() !== "" && (name.text !== backend.projectName || number.text !== backend.projectNumber))
            backend.renameProject(name.text, number.text)
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 8

        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TextField {
                id: number
                Layout.preferredWidth: 120
                placeholderText: qsTr("Number")
                onEditingFinished: dialog.saveProject()
            }
            TextField {
                id: name
                Layout.fillWidth: true
                placeholderText: qsTr("Project name")
                onEditingFinished: dialog.saveProject()
            }
        }

        Label {
            Layout.topMargin: 10
            text: qsTr("Team")
            color: "#7a7a84"
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
        }

        Repeater {
            model: backend.parties
            delegate: RowLayout {
                id: party
                required property string modelData
                required property int index
                Layout.fillWidth: true
                spacing: 2
                TextField {
                    Layout.fillWidth: true
                    text: party.modelData
                    onEditingFinished: if (text.trim() !== party.modelData && !backend.renameParty(party.modelData, text)) text = party.modelData
                }
                ToolButton {
                    text: "↑"
                    enabled: party.index > 0
                    onClicked: backend.moveParty(party.modelData, -1)
                }
                ToolButton {
                    text: "↓"
                    enabled: party.index < backend.parties.length - 1
                    onClicked: backend.moveParty(party.modelData, 1)
                }
                ToolButton {
                    text: "✕"
                    onClicked: backend.removeParty(party.modelData)
                }
            }
        }

        TextField {
            id: added
            Layout.fillWidth: true
            placeholderText: qsTr("Add a party, such as Structural Engineer")
            onAccepted: if (backend.addParty(text)) text = ""
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: 4
            text: qsTr("New items go from the first party to the second.")
            color: "#6c6c76"
            font.pixelSize: 12
            wrapMode: Text.Wrap
        }
    }

    footer: DialogButtonBox {
        background: Item {}
        Button { text: qsTr("Done"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
    }
    onAccepted: saveProject()
}
