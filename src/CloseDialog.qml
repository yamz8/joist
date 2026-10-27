import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Answers an RFI or records a submittal review. Revise and resubmit sends
// a submittal back to whoever submitted it as its next revision.
PanelDialog {
    id: dialog
    property var item: null
    property var actions: []
    readonly property bool rfi: item !== null && item.type === "rfi"

    width: 520
    title: !item ? "" : rfi ? qsTr("Answer %1").arg(item.code) : qsTr("Review %1").arg(item.code)

    function pick(next) {
        item = next
        actions = backend.closingActions(next.type)
        list.currentIndex = 0
        answer.text = next.answer
        note.text = ""
        open()
    }

    onOpened: list.forceActiveFocus()
    onAccepted: backend.closeItem(item.code, actions[list.currentIndex], answer.text, note.text)

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            interactive: false
            model: dialog.actions
            keyNavigationEnabled: true
            Keys.onReturnPressed: dialog.accept()
            Keys.onEnterPressed: dialog.accept()
            Keys.onPressed: (event) => {
                const n = parseInt(event.text)
                if (n >= 1 && n <= dialog.actions.length) {
                    currentIndex = n - 1
                    event.accepted = true
                }
            }
            delegate: ItemDelegate {
                required property string modelData
                required property int index
                width: list.width
                highlighted: ListView.isCurrentItem
                text: (index + 1) + "   " + win.actionText(modelData)
                onClicked: list.currentIndex = index
            }
        }

        Label {
            Layout.fillWidth: true
            visible: dialog.actions[list.currentIndex] === "revise-resubmit"
            text: dialog.item ? qsTr("Goes back to %1 as revision %2.")
                .arg(dialog.item.originator || backend.parties[0]).arg(dialog.item.revision + 1) : ""
            color: "#8a8a94"
            wrapMode: Text.Wrap
        }

        TextArea {
            id: answer
            Layout.fillWidth: true
            Layout.preferredHeight: 130
            wrapMode: TextEdit.Wrap
            placeholderText: dialog.rfi ? qsTr("Answer") : qsTr("Review comments")
            Keys.onPressed: (event) => {
                if ((event.key === Qt.Key_Return || event.key === Qt.Key_Enter) && (event.modifiers & Qt.ControlModifier)) {
                    dialog.accept()
                    event.accepted = true
                }
            }
        }

        TextField {
            id: note
            Layout.fillWidth: true
            placeholderText: qsTr("Note for the history (optional)")
            onAccepted: dialog.accept()
        }
    }

    footer: DialogButtonBox {
        background: Item {}
        Button { text: qsTr("Cancel"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
        AccentButton {
            text: dialog.actions[list.currentIndex] === "revise-resubmit" ? qsTr("Send back") : qsTr("Close item")
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
        }
    }
}
