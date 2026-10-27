import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Puts the ball in another party's court, with a new due date. Up and Down
// (or a number) pick the party; Enter passes.
PanelDialog {
    id: dialog
    property var item: null
    property var choices: []

    width: 440
    title: item ? qsTr("Pass %1 to").arg(item.code) : ""

    function pick(next) {
        item = next
        choices = backend.parties.filter(party => party !== next.ballInCourt)
        if (choices.length === 0) {
            backend.reportError(qsTr("Add someone else to the team first (T)."))
            return
        }
        list.currentIndex = Math.max(0, choices.indexOf(backend.suggestedRecipient(next.code)))
        days.value = backend.defaultDays(next.type)
        note.text = ""
        open()
    }

    onOpened: list.forceActiveFocus()
    onAccepted: backend.passTo(item.code, choices[list.currentIndex], days.value, note.text)

    ColumnLayout {
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: 12

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.preferredHeight: Math.min(contentHeight, 260)
            clip: true
            model: dialog.choices
            keyNavigationEnabled: true
            Keys.onReturnPressed: dialog.accept()
            Keys.onEnterPressed: dialog.accept()
            Keys.onPressed: (event) => {
                const n = parseInt(event.text)
                if (n >= 1 && n <= dialog.choices.length) {
                    currentIndex = n - 1
                    event.accepted = true
                }
            }
            delegate: ItemDelegate {
                required property string modelData
                required property int index
                width: list.width
                highlighted: ListView.isCurrentItem
                text: (index < 9 ? (index + 1) + "   " : "    ") + modelData
                onClicked: list.currentIndex = index
                onDoubleClicked: dialog.accept()
            }
        }

        // The language's own "Due in %n days", split around the number box,
        // so every language keeps its word order.
        RowLayout {
            spacing: 10
            readonly property string sentence: qsTr("Due in %n days", "", days.value)
            readonly property int at: sentence.indexOf(String(days.value))
            Label {
                text: parent.at < 0 ? parent.sentence : parent.sentence.substring(0, parent.at).trim()
                visible: text !== ""
            }
            SpinBox {
                id: days
                from: 0
                to: 365
                editable: true
                Keys.onReturnPressed: dialog.accept()
            }
            Label {
                text: parent.at < 0 ? "" : parent.sentence.substring(parent.at + String(days.value).length).trim()
                visible: text !== ""
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
        AccentButton { text: qsTr("Pass"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
    }
}
