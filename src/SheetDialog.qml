import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Picks a sheet from a drawing set Lintel has listed. Typing filters the
// labels; Up and Down move; Enter links.
PanelDialog {
    id: dialog
    property string code
    property string file
    property var sheets: []
    readonly property var shown: sheets.filter(sheet => sheet.toLowerCase().indexOf(filter.text.trim().toLowerCase()) >= 0)

    width: 420
    title: qsTr("Link %1 to a sheet").arg(code)

    function pick(nextCode, nextFile, nextSheets) {
        code = nextCode
        file = nextFile
        sheets = nextSheets
        filter.text = ""
        list.currentIndex = 0
        open()
    }

    onOpened: filter.forceActiveFocus()
    onAccepted: if (shown.length > 0) backend.addSheetLink(code, file, shown[Math.max(0, list.currentIndex)])

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 10

        Label {
            Layout.fillWidth: true
            text: dialog.file.substring(dialog.file.lastIndexOf("/") + 1)
            color: "#8a8a94"
            elide: Text.ElideMiddle
        }

        TextField {
            id: filter
            Layout.fillWidth: true
            placeholderText: qsTr("Sheet, such as A-101")
            onTextChanged: list.currentIndex = 0
            onAccepted: dialog.accept()
            Keys.onDownPressed: list.incrementCurrentIndex()
            Keys.onUpPressed: list.decrementCurrentIndex()
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: 300
            clip: true
            model: dialog.shown
            ScrollBar.vertical: ScrollBar {}
            delegate: ItemDelegate {
                required property string modelData
                required property int index
                width: list.width
                highlighted: ListView.isCurrentItem
                text: modelData
                onClicked: list.currentIndex = index
                onDoubleClicked: dialog.accept()
            }
        }
    }

    footer: DialogButtonBox {
        background: Item {}
        Button { text: qsTr("Cancel"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        AccentButton { text: qsTr("Link"); enabled: dialog.shown.length > 0; DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
    }
}
