import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: win
    width: 1320
    height: 840
    minimumWidth: 900
    minimumHeight: 520
    visible: true
    title: backend.hasProject ? "Joist — " + backend.projectName : "Joist"
    color: "#0e0e10"

    Material.theme: Material.Dark
    Material.accent: theme.accent
    Material.roundedScale: theme.radius

    // The view: "board" (ball in court), "rfi", or "submittal".
    property string view: "board"
    property string selected: ""
    property bool helpVisible: false
    property bool showClosed: true
    property string notice: ""
    readonly property var selectedItem: {
        backend.items
        return selected ? backend.item(selected) : null
    }
    readonly property bool hasItem: selectedItem !== null && selectedItem.code !== undefined
    readonly property bool dialogOpen: passDialog.visible || closeDialog.visible || sheetDialog.visible
        || teamDialog.visible || projectPicker.visible || deleteDialog.visible
    // Single-key shortcuts pause while a dialog is up.
    readonly property bool keys: !dialogOpen && backend.hasProject
    readonly property var codes: view === "board" ? board.codes : log.codes

    // Dates and counts re-render when Omarchy's language changes.
    function date(iso) {
        if (!iso || translator.language === "")
            return ""
        return new Date(iso + "T00:00:00").toLocaleDateString(Qt.locale(), Locale.ShortFormat)
    }

    function dueText(item) {
        if (!item.open)
            return qsTr("Closed %1").arg(date(item.closed))
        if (item.daysLeft === undefined || item.daysLeft === null)
            return qsTr("No due date")
        if (item.daysLeft < 0)
            return qsTr("%n days overdue", "", -item.daysLeft)
        if (item.daysLeft === 0)
            return qsTr("Due today")
        return qsTr("Due in %n days", "", item.daysLeft)
    }

    function dueColor(item) {
        if (!item.open)
            return "#8a8a94"
        if (item.overdue)
            return "#ff6b6b"
        return item.daysLeft === 0 ? theme.accent : "#b8b8c0"
    }

    function show(next) {
        view = next
        if (selected && view !== "board" && selectedItem && selectedItem.type !== view)
            selected = ""
    }

    function step(by) {
        if (codes.length === 0)
            return
        const at = codes.indexOf(selected)
        const next = at < 0 ? (by > 0 ? 0 : codes.length - 1) : Math.max(0, Math.min(codes.length - 1, at + by))
        selected = codes[next]
    }

    function add(type) {
        const code = backend.addItem(type)
        if (!code)
            return
        if (view !== "board")
            view = type
        selected = code
        panel.editTitle()
    }

    function pass() {
        if (hasItem && selectedItem.open)
            passDialog.pick(selectedItem)
    }

    function closeOrReopen() {
        if (!hasItem)
            return
        if (selectedItem.open)
            closeDialog.pick(selectedItem)
        else
            backend.reopen(selected)
    }

    function exportView() {
        backend.exportLog(view)
    }

    function say(text) {
        notice = text
        noticeTimer.restart()
    }

    // Leaves a text field, so single-key shortcuts work again.
    function leaveField() {
        keyCatcher.forceActiveFocus()
    }

    Connections {
        target: backend
        function onExported(path) { win.say(qsTr("Exported %1").arg(path.substring(path.lastIndexOf("/") + 1))) }
        function onSheetsListed(code, file, sheets) { sheetDialog.pick(code, file, sheets) }
        function onProjectChanged() {
            if (win.selected && !backend.item(win.selected).code)
                win.selected = ""
        }
    }

    Timer {
        id: noticeTimer
        interval: 4000
        onTriggered: win.notice = ""
    }

    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: win.helpVisible = !win.helpVisible }
    // Everything saves as it changes, so there's nothing to lose by quitting.
    Shortcut { sequence: "Q"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: Qt.quit() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.show("board") }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.show("rfi") }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.show("submittal") }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.add("rfi") }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.add("submittal") }
    Shortcut { sequences: ["J", "Down"]; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.step(1) }
    Shortcut { sequences: ["K", "Up"]; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.step(-1) }
    Shortcut { sequence: "Space"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.pass() }
    Shortcut { sequence: "C"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.closeOrReopen() }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: backend.shiftDue(win.selected, 1) }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: backend.shiftDue(win.selected, -1) }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: backend.linkSheet(win.selected) }
    Shortcut {
        sequence: "O"
        context: Qt.ApplicationShortcut
        enabled: win.keys && win.hasItem && win.selectedItem.sheets.length > 0
        onActivated: backend.openSheet(win.selectedItem.sheets[0].file, win.selectedItem.sheets[0].sheet)
    }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.exportView() }
    Shortcut { sequence: "F"; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: backend.exportForm(win.selected) }
    Shortcut { sequence: "H"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: win.showClosed = !win.showClosed }
    Shortcut { sequence: "/"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: search.forceActiveFocus() }
    Shortcut { sequence: "Return"; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: panel.editTitle() }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: teamDialog.open() }
    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: !win.dialogOpen; onActivated: projectPicker.open() }
    Shortcut { sequences: [StandardKey.Delete]; context: Qt.ApplicationShortcut; enabled: win.keys && win.hasItem; onActivated: deleteDialog.open() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo, "Ctrl+Shift+Z"]; context: Qt.ApplicationShortcut; enabled: win.keys; onActivated: backend.redo() }
    // Escape steps back one thing at a time: help, the search, the
    // selection, then the error. Open dialogs close themselves.
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: !win.dialogOpen
        onActivated: {
            if (win.helpVisible)
                win.helpVisible = false
            else if (search.text !== "")
                search.text = ""
            else if (win.selected)
                win.selected = ""
            else
                backend.clearError()
        }
    }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        id: keyCatcher
        anchors.fill: parent
        focus: true
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // The bar: the project, the views, and the actions.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                color: "#131316"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 10
                    spacing: 10

                    Label {
                        text: "Joist"
                        font.pixelSize: 16
                        font.bold: true
                        color: theme.accent
                    }

                    ToolButton {
                        text: backend.hasProject
                            ? (backend.projectNumber ? backend.projectNumber + " · " : "") + backend.projectName + "  ▾"
                            : qsTr("Projects") + "  ▾"
                        font.pixelSize: 14
                        onClicked: projectPicker.open()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Switch project (P)")
                    }

                    Item { Layout.preferredWidth: 6 }

                    Repeater {
                        model: backend.hasProject ? [
                            { view: "board", label: qsTr("Ball in court"), key: "1" },
                            { view: "rfi", label: qsTr("RFIs"), key: "2" },
                            { view: "submittal", label: qsTr("Submittals"), key: "3" }
                        ] : []
                        delegate: TabLink {
                            required property var modelData
                            label: modelData.label
                            count: win.countFor(modelData.view)
                            late: win.lateFor(modelData.view)
                            active: win.view === modelData.view
                            onClicked: win.show(modelData.view)
                        }
                    }

                    Item { Layout.fillWidth: true }

                    TextField {
                        id: search
                        visible: backend.hasProject
                        // Gives way first when long translations crowd the bar.
                        Layout.fillWidth: true
                        Layout.maximumWidth: 220
                        Layout.minimumWidth: 90
                        implicitHeight: 34
                        topPadding: 6
                        bottomPadding: 6
                        leftPadding: 12
                        font.pixelSize: 13
                        placeholderText: qsTr("Search (/)")
                        placeholderTextColor: "#6c6c76"
                        // A compact box, not Material's tall outlined field.
                        background: Rectangle {
                            radius: Math.min(6, theme.radius)
                            color: "#1a1a1e"
                            border.color: search.activeFocus ? theme.accent : "#2a2a30"
                        }
                        Keys.onEscapePressed: { text = ""; win.leaveField() }
                        Keys.onReturnPressed: win.leaveField()
                    }

                    Button {
                        visible: backend.hasProject
                        text: qsTr("New RFI")
                        flat: true
                        onClicked: win.add("rfi")
                        ToolTip.visible: hovered
                        ToolTip.text: "R"
                    }
                    Button {
                        visible: backend.hasProject
                        text: qsTr("New submittal")
                        flat: true
                        onClicked: win.add("submittal")
                        ToolTip.visible: hovered
                        ToolTip.text: "S"
                    }
                    Button {
                        visible: backend.hasProject
                        text: qsTr("Export")
                        flat: true
                        onClicked: win.exportView()
                        ToolTip.visible: hovered
                        ToolTip.text: qsTr("Export this view as PDF (E)")
                    }
                    ToolButton {
                        text: "?"
                        onClicked: win.helpVisible = !win.helpVisible
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: "#232328" }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0
                visible: backend.hasProject

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    BoardView {
                        id: board
                        anchors.fill: parent
                        visible: win.view === "board"
                        filter: search.text
                        selected: win.selected
                        onPicked: (code) => win.selected = code
                    }

                    LogView {
                        id: log
                        anchors.fill: parent
                        visible: win.view !== "board"
                        type: win.view === "board" ? "rfi" : win.view
                        filter: search.text
                        showClosed: win.showClosed
                        selected: win.selected
                        onPicked: (code) => win.selected = code
                    }
                }

                Rectangle {
                    Layout.preferredWidth: 1
                    Layout.fillHeight: true
                    color: "#232328"
                    visible: panel.visible
                }

                ItemPanel {
                    id: panel
                    Layout.preferredWidth: Math.min(460, win.width * 0.42)
                    Layout.fillHeight: true
                    visible: win.hasItem
                    item: win.hasItem ? win.selectedItem : null
                }
            }

            StartView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: !backend.hasProject
            }

            // Errors stay until dismissed; they say what's wrong.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: errorText.implicitHeight + 20
                visible: backend.error !== ""
                color: "#3a1518"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 16
                    anchors.rightMargin: 8
                    Label {
                        id: errorText
                        Layout.fillWidth: true
                        text: backend.error
                        color: "#ffb4b4"
                        wrapMode: Text.Wrap
                    }
                    ToolButton { text: "✕"; onClicked: backend.clearError() }
                }
            }
        }

        // Says how an export went, for a few seconds.
        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 24
            visible: win.notice !== ""
            width: noticeLabel.implicitWidth + 32
            height: 38
            radius: theme.radius
            color: "#202024"
            border.color: "#34343a"
            Label {
                id: noticeLabel
                anchors.centerIn: parent
                text: win.notice
            }
        }

        ShortcutHelp {
            anchors.fill: parent
            visible: win.helpVisible
        }
    }

    function countFor(view) {
        let count = 0
        for (const item of backend.items)
            if (item.open && (view === "board" || item.type === view))
                ++count
        return count
    }

    function lateFor(view) {
        let count = 0
        for (const item of backend.items)
            if (item.overdue && (view === "board" || item.type === view))
                ++count
        return count
    }

    PassDialog { id: passDialog }
    CloseDialog { id: closeDialog }
    SheetDialog { id: sheetDialog }
    TeamDialog { id: teamDialog }
    ProjectPicker { id: projectPicker }

    PanelDialog {
        id: deleteDialog
        width: 440
        title: win.hasItem ? qsTr("Delete %1?").arg(win.selected) : ""
        footer: DialogButtonBox {
            background: Item {}
            Button { text: qsTr("Cancel"); flat: true; DialogButtonBox.buttonRole: DialogButtonBox.RejectRole }
            AccentButton { id: deleteButton; text: qsTr("Delete"); DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole }
        }
        Label {
            text: qsTr("Its history and sheet links go with it. Ctrl+Z brings it back.")
            wrapMode: Text.Wrap
            width: deleteDialog.availableWidth
        }
        onAccepted: {
            backend.deleteItem(win.selected)
            win.selected = ""
        }
        onOpened: deleteButton.forceActiveFocus()
    }
}
