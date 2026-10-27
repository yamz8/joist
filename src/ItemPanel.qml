import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// The picked item: who holds it, what it asks, the sheets it points at, and
// how it got here. Edits save when a field is left.
Rectangle {
    id: panel
    property var item: null
    readonly property bool rfi: item !== null && item.type === "rfi"
    color: "#111114"
    Material.roundedScale: theme.radius

    function editTitle() {
        Qt.callLater(() => { title.forceActiveFocus(); title.selectAll() })
    }

    component SectionLabel: Label {
        color: "#7a7a84"
        font.pixelSize: 11
        font.capitalization: Font.AllUppercase
        font.letterSpacing: 0.5
        Layout.topMargin: 10
    }

    // A field that saves to one of the item's fields when left. It remembers
    // which item it was editing, so moving to another item saves the right one.
    component FieldEdit: TextField {
        id: field
        property string field
        property string value
        property string editing
        text: value
        Layout.fillWidth: true
        onActiveFocusChanged: if (activeFocus) editing = panel.item.code
        onEditingFinished: if (editing && text !== value) backend.setField(editing, field.field, text)
        Keys.onEscapePressed: { text = value; win.leaveField() }
        Keys.onReturnPressed: win.leaveField()
    }

    component AreaEdit: TextArea {
        id: area
        property string field
        property string value
        property string editing
        text: value
        Layout.fillWidth: true
        Layout.minimumHeight: 90
        wrapMode: TextEdit.Wrap
        onActiveFocusChanged: {
            if (activeFocus)
                editing = panel.item.code
            else if (editing && text !== value)
                backend.setField(editing, area.field, text)
        }
        Keys.onEscapePressed: win.leaveField()
    }

    ScrollView {
        id: scroll
        anchors.fill: parent
        contentWidth: availableWidth
        visible: panel.item !== null

        ColumnLayout {
            width: scroll.availableWidth - 40
            x: 20
            spacing: 8

            Item { Layout.preferredHeight: 10 }

            // The number, its kind, and its state.
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Label {
                    text: panel.item ? panel.item.code : ""
                    font.family: "monospace"
                    font.pixelSize: 20
                    font.bold: true
                    color: theme.accent
                }
                Label {
                    text: panel.item ? win.typeText(panel.item.type)
                        + (panel.item.type === "submittal" ? " · " + qsTr("Rev. %1").arg(panel.item.revision) : "") : ""
                    color: "#8a8a94"
                }
                Item { Layout.fillWidth: true }
                Rectangle {
                    implicitWidth: status.implicitWidth + 16
                    implicitHeight: 24
                    radius: Math.min(12, theme.radius)
                    color: !panel.item ? "transparent" : panel.item.overdue ? "#5a1d22" : panel.item.open ? "#23232a" : "#1c2a20"
                    Label {
                        id: status
                        anchors.centerIn: parent
                        text: panel.item ? win.statusText(panel.item) : ""
                        font.pixelSize: 12
                        color: !panel.item ? "white" : panel.item.overdue ? "#ff9a9a" : panel.item.open ? "#d8d8de" : "#9ad5a8"
                    }
                }
            }

            // Ball in court: the one thing this app is about.
            Rectangle {
                Layout.fillWidth: true
                Layout.topMargin: 6
                implicitHeight: court.implicitHeight + 28
                radius: theme.radius
                color: "#18181c"
                border.color: panel.item && panel.item.overdue ? "#7a2a30" : "#26262c"

                ColumnLayout {
                    id: court
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 6

                    Label {
                        text: panel.item && panel.item.open ? qsTr("Ball in court") : qsTr("Closed")
                        color: "#7a7a84"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                    }
                    Label {
                        Layout.fillWidth: true
                        text: !panel.item ? "" : panel.item.open ? panel.item.ballInCourt
                            : win.actionText(panel.item.action)
                        font.pixelSize: 22
                        font.bold: true
                        elide: Text.ElideRight
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 4
                        Label {
                            text: panel.item ? win.dueText(panel.item) : ""
                            color: panel.item ? win.dueColor(panel.item) : "white"
                            font.bold: panel.item !== null && panel.item.overdue
                        }
                        Label {
                            text: panel.item && panel.item.open && panel.item.due
                                ? "· " + win.date(panel.item.due) : ""
                            color: "#7a7a84"
                        }
                        ToolButton {
                            visible: panel.item !== null && panel.item.open
                            text: "−"
                            onClicked: backend.shiftDue(panel.item.code, -1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("A day earlier ([)")
                        }
                        ToolButton {
                            visible: panel.item !== null && panel.item.open
                            text: "+"
                            onClicked: backend.shiftDue(panel.item.code, 1)
                            ToolTip.visible: hovered
                            ToolTip.text: qsTr("A day later (])")
                        }
                        Item { Layout.fillWidth: true }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        AccentButton {
                            visible: panel.item !== null && panel.item.open
                            text: qsTr("Pass to…")
                            onClicked: win.pass()
                            ToolTip.visible: hovered
                            ToolTip.text: "Space"
                        }
                        Button {
                            text: panel.item && panel.item.open
                                ? (panel.rfi ? qsTr("Answer…") : qsTr("Review…")) : qsTr("Reopen")
                            flat: true
                            onClicked: win.closeOrReopen()
                            ToolTip.visible: hovered
                            ToolTip.text: "C"
                        }
                    }
                }
            }

            // Material fields float their placeholder as the label.
            FieldEdit {
                id: title
                Layout.topMargin: 12
                field: "title"
                value: panel.item ? panel.item.title : ""
                placeholderText: qsTr("Subject")
                font.pixelSize: 15
            }

            FieldEdit {
                field: "spec"
                value: panel.item ? panel.item.spec : ""
                placeholderText: qsTr("Spec section")
            }

            AreaEdit {
                field: "question"
                value: panel.item ? panel.item.question : ""
                placeholderText: panel.rfi ? qsTr("Question") : qsTr("Description")
            }

            AreaEdit {
                field: "answer"
                value: panel.item ? panel.item.answer : ""
                placeholderText: panel.rfi ? qsTr("Answer") : qsTr("Review comments")
            }

            // Lintel drawing sheets this item points at.
            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 10
                SectionLabel { text: qsTr("Drawing sheets"); Layout.topMargin: 0 }
                Item { Layout.fillWidth: true }
                BusyIndicator {
                    running: backend.listingSheets
                    visible: running
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                }
                Button {
                    flat: true
                    text: qsTr("Link a sheet…")
                    onClicked: backend.linkSheet(panel.item.code)
                    ToolTip.visible: hovered
                    ToolTip.text: "L"
                }
            }
            Repeater {
                model: panel.item ? panel.item.sheets : []
                delegate: Rectangle {
                    id: sheetRow
                    required property var modelData
                    required property int index
                    Layout.fillWidth: true
                    implicitHeight: 44
                    radius: Math.min(4, theme.radius)
                    color: sheetHover.hovered ? "#1d1d22" : "#18181c"
                    HoverHandler { id: sheetHover }

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 12
                        anchors.rightMargin: 4
                        spacing: 10
                        Label {
                            text: sheetRow.modelData.sheet
                            font.bold: true
                            color: theme.accent
                        }
                        Label {
                            Layout.fillWidth: true
                            text: sheetRow.modelData.exists ? sheetRow.modelData.fileName
                                : qsTr("%1 (missing)").arg(sheetRow.modelData.fileName)
                            color: sheetRow.modelData.exists ? "#9a9aa4" : "#ff6b6b"
                            elide: Text.ElideMiddle
                        }
                        ToolButton {
                            text: qsTr("Open")
                            enabled: sheetRow.modelData.exists
                            onClicked: backend.openSheet(sheetRow.modelData.file, sheetRow.modelData.sheet)
                            ToolTip.visible: hovered && sheetRow.index === 0
                            ToolTip.text: qsTr("Open in Lintel (O)")
                        }
                        ToolButton {
                            text: "✕"
                            onClicked: backend.removeSheetLink(panel.item.code, sheetRow.index)
                        }
                    }
                }
            }
            Label {
                visible: panel.item !== null && panel.item.sheets.length === 0
                text: qsTr("No sheets linked.")
                color: "#5c5c66"
            }

            SectionLabel { text: qsTr("History") }
            Repeater {
                model: panel.item ? panel.item.history.slice().reverse() : []
                delegate: RowLayout {
                    id: event
                    required property var modelData
                    Layout.fillWidth: true
                    spacing: 10
                    Label {
                        Layout.preferredWidth: 84
                        Layout.alignment: Qt.AlignTop
                        text: win.date(event.modelData.date)
                        color: "#7a7a84"
                        font.pixelSize: 12
                    }
                    Label {
                        Layout.fillWidth: true
                        wrapMode: Text.Wrap
                        font.pixelSize: 13
                        color: "#c8c8d0"
                        text: {
                            const e = event.modelData
                            let line = e.from && e.to ? qsTr("%1 → %2").arg(e.from).arg(e.to) : (e.to || e.from)
                            if (e.action)
                                line = win.actionText(e.action) + " · " + line
                            return e.note ? line + "\n" + e.note : line
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: 14
                Button {
                    flat: true
                    text: qsTr("Export form")
                    onClicked: backend.exportForm(panel.item.code)
                    ToolTip.visible: hovered
                    ToolTip.text: "F"
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    text: qsTr("Delete")
                    Material.foreground: "#ff8a8a"
                    onClicked: deleteDialog.open()
                }
            }

            Item { Layout.preferredHeight: 16 }
        }
    }
}
