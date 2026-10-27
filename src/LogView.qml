import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// The RFI or submittal log: every item, numbered, as it would print.
Item {
    id: log
    property string type: "rfi"
    property string filter
    property bool showClosed: true
    property string selected
    signal picked(string code)

    readonly property bool submittals: type === "submittal"
    readonly property var rows: backend.items.filter(item => {
        if (item.type !== type || (!showClosed && !item.open))
            return false
        const text = filter.trim().toLowerCase()
        return text === "" || [item.code, item.title, item.spec, item.ballInCourt, win.statusText(item)]
            .some(value => value.toLowerCase().indexOf(text) >= 0)
    })
    readonly property var codes: rows.map(item => item.code)

    onSelectedChanged: {
        const at = codes.indexOf(selected)
        if (at >= 0)
            list.positionViewAtIndex(at, ListView.Contain)
    }

    // Column widths; the subject takes what's left.
    // Narrow logs drop the spec column before squeezing the subject.
    readonly property var widths: ({
        code: 84, rev: submittals ? 44 : 0, spec: width > 980 ? 90 : 0,
        holder: width > 820 ? 150 : 120, due: 130, status: width > 820 ? 130 : 100
    })

    component Cell: Label {
        elide: Text.ElideRight
        verticalAlignment: Text.AlignVCenter
        Layout.fillHeight: true
        font.pixelSize: 13
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#111114"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 20
                spacing: 12
                Repeater {
                    model: [
                        [qsTr("No."), log.widths.code],
                        [qsTr("Rev."), log.widths.rev],
                        [qsTr("Subject"), -1],
                        [qsTr("Spec"), log.widths.spec],
                        [qsTr("Ball in court"), log.widths.holder],
                        [qsTr("Due"), log.widths.due],
                        [qsTr("Status"), log.widths.status]
                    ]
                    delegate: Label {
                        required property var modelData
                        visible: modelData[1] !== 0
                        Layout.preferredWidth: modelData[1] > 0 ? modelData[1] : -1
                        Layout.fillWidth: modelData[1] < 0
                        text: modelData[0]
                        color: "#7a7a84"
                        font.pixelSize: 11
                        font.capitalization: Font.AllUppercase
                        font.letterSpacing: 0.5
                        elide: Text.ElideRight
                    }
                }
            }
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: log.rows
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar {}

            delegate: AbstractButton {
                id: row
                required property var modelData
                width: list.width
                height: 44
                hoverEnabled: true
                readonly property bool picked: modelData.code === log.selected
                onPressed: win.leaveField()
                onClicked: log.picked(modelData.code)

                background: Rectangle {
                    color: row.picked ? Qt.rgba(Qt.color(theme.accent).r, Qt.color(theme.accent).g, Qt.color(theme.accent).b, 0.12)
                                      : row.hovered ? "#16161a" : "transparent"
                    Rectangle {
                        anchors.left: parent.left
                        width: 3
                        height: parent.height
                        color: row.picked ? theme.accent : (row.modelData.overdue ? "#ff6b6b" : "transparent")
                    }
                    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: "#1c1c21" }
                }

                contentItem: RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.rightMargin: 20
                    spacing: 12
                    opacity: row.modelData.open ? 1 : 0.6

                    Cell {
                        Layout.preferredWidth: log.widths.code
                        text: row.modelData.code
                        font.family: "monospace"
                        color: row.picked ? theme.accent : "#a8a8b2"
                    }
                    Cell {
                        visible: log.submittals
                        Layout.preferredWidth: log.widths.rev
                        text: row.modelData.revision.toLocaleString(Qt.locale(), 'f', 0)
                        color: "#a8a8b2"
                    }
                    Cell {
                        Layout.fillWidth: true
                        text: row.modelData.title || qsTr("Untitled")
                        color: row.modelData.title ? "#ececf0" : "#6c6c76"
                        font.pixelSize: 14
                    }
                    Cell {
                        visible: log.widths.spec > 0
                        Layout.preferredWidth: log.widths.spec
                        text: row.modelData.spec
                        color: "#9a9aa4"
                    }
                    Cell {
                        Layout.preferredWidth: log.widths.holder
                        text: row.modelData.ballInCourt
                        color: "#d8d8de"
                    }
                    Cell {
                        Layout.preferredWidth: log.widths.due
                        text: win.dueText(row.modelData)
                        color: win.dueColor(row.modelData)
                        font.bold: row.modelData.overdue
                    }
                    Cell {
                        Layout.preferredWidth: log.widths.status
                        text: win.statusText(row.modelData)
                        color: row.modelData.overdue ? "#ff6b6b" : row.modelData.open ? "#d8d8de" : "#9a9aa4"
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: list.count === 0
                horizontalAlignment: Text.AlignHCenter
                color: "#6c6c76"
                text: log.filter ? qsTr("No matches")
                    : log.submittals ? qsTr("No submittals yet. Press S to log one.")
                    : qsTr("No RFIs yet. Press R to log one.")
            }
        }

        // What the log holds, and whether closed items show.
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            color: "#111114"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 20
                anchors.rightMargin: 12
                Label {
                    readonly property var all: backend.items.filter(item => item.type === log.type)
                    text: qsTr("%n items", "", all.length) + " · "
                        + qsTr("%n open", "", all.filter(item => item.open).length) + " · "
                        + qsTr("%n overdue", "", all.filter(item => item.overdue).length)
                    color: "#7a7a84"
                    font.pixelSize: 12
                }
                Item { Layout.fillWidth: true }
                Button {
                    flat: true
                    font.pixelSize: 12
                    text: log.showClosed ? qsTr("Hide closed (H)") : qsTr("Show closed (H)")
                    onClicked: win.showClosed = !win.showClosed
                }
            }
        }
    }
}
