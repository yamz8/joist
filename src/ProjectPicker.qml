import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every project, with what's open and what's late. Enter opens the
// highlighted one; the fields below start a new one.
PanelDialog {
    id: dialog
    width: 520
    title: qsTr("Projects")

    onOpened: {
        name.text = ""
        number.text = ""
        list.currentIndex = Math.max(0, backend.projects.findIndex(project => project.file === backend.projectFile))
        if (backend.projects.length > 0)
            list.forceActiveFocus()
        else
            name.forceActiveFocus()
    }
    onClosed: win.leaveField()

    function openCurrent() {
        if (list.currentIndex >= 0 && list.currentIndex < backend.projects.length) {
            win.selected = ""
            backend.openProject(backend.projects[list.currentIndex].file)
            close()
        }
    }

    function create() {
        if (backend.createProject(name.text, number.text)) {
            win.selected = ""
            close()
        }
    }

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 10

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 320)
            visible: count > 0
            clip: true
            model: backend.projects
            keyNavigationEnabled: true
            Keys.onReturnPressed: dialog.openCurrent()
            Keys.onEnterPressed: dialog.openCurrent()
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                id: row
                required property var modelData
                required property int index
                width: list.width
                highlighted: ListView.isCurrentItem
                onClicked: { list.currentIndex = index; dialog.openCurrent() }
                contentItem: RowLayout {
                    spacing: 10
                    Label {
                        text: row.modelData.number
                        visible: text !== ""
                        font.family: "monospace"
                        color: "#9a9aa4"
                    }
                    Label {
                        Layout.fillWidth: true
                        text: row.modelData.name
                        font.bold: row.modelData.file === backend.projectFile
                        elide: Text.ElideRight
                    }
                    Label {
                        text: qsTr("%n open", "", row.modelData.open)
                        color: "#8a8a94"
                        font.pixelSize: 12
                    }
                    Label {
                        text: qsTr("%n overdue", "", row.modelData.overdue)
                        visible: row.modelData.overdue > 0
                        color: "#ff6b6b"
                        font.pixelSize: 12
                    }
                }
            }
        }

        Label {
            Layout.topMargin: list.visible ? 10 : 0
            text: qsTr("New project")
            color: "#7a7a84"
            font.pixelSize: 11
            font.capitalization: Font.AllUppercase
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 10
            TextField {
                id: number
                Layout.preferredWidth: 120
                placeholderText: qsTr("Number")
                onAccepted: dialog.create()
            }
            TextField {
                id: name
                Layout.fillWidth: true
                placeholderText: qsTr("Project name")
                onAccepted: dialog.create()
            }
            AccentButton {
                text: qsTr("Create")
                enabled: name.text.trim() !== ""
                onClicked: dialog.create()
            }
        }
    }
}
