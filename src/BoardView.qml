import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Ball in court: a column per party, holding the open items they owe an
// answer on, most urgent first.
Item {
    id: board
    property string filter
    property string selected
    signal picked(string code)

    function matches(item) {
        const text = filter.trim().toLowerCase()
        return text === "" || [item.code, item.title, item.spec, item.ballInCourt]
            .some(value => value.toLowerCase().indexOf(text) >= 0)
    }

    readonly property var columns: backend.board.map(column => ({
        party: column.party,
        overdue: column.overdue,
        total: column.items.length,
        items: column.items.filter(matches)
    }))

    // The order J and K walk through: down each column, left to right.
    readonly property var codes: {
        const list = []
        for (const column of columns)
            for (const item of column.items)
                list.push(item.code)
        return list
    }

    readonly property real columnWidth: Math.max(250, (width - 24 - (columns.length - 1) * 12) / Math.max(1, columns.length))

    Flickable {
        id: flick
        anchors.fill: parent
        anchors.margins: 12
        contentWidth: row.width
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        ScrollBar.horizontal: ScrollBar {}

        Row {
            id: row
            height: flick.height
            spacing: 12

            Repeater {
                model: board.columns
                delegate: Rectangle {
                    id: column
                    required property var modelData
                    width: board.columnWidth
                    height: row.height
                    radius: theme.radius
                    color: "#131316"
                    border.color: "#1f1f24"

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.leftMargin: 4
                            Layout.rightMargin: 4
                            Label {
                                Layout.fillWidth: true
                                text: column.modelData.party || qsTr("Nobody")
                                font.pixelSize: 15
                                font.bold: true
                                elide: Text.ElideRight
                            }
                            Label {
                                text: qsTr("%n overdue", "", column.modelData.overdue)
                                visible: column.modelData.overdue > 0
                                color: "#ff6b6b"
                                font.pixelSize: 12
                            }
                            Label {
                                text: column.modelData.total.toLocaleString(Qt.locale(), 'f', 0)
                                color: "#8a8a94"
                                font.pixelSize: 13
                            }
                        }

                        ListView {
                            id: cards
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            clip: true
                            spacing: 8
                            model: column.modelData.items
                            boundsBehavior: Flickable.StopAtBounds
                            ScrollBar.vertical: ScrollBar {}
                            delegate: BoardCard {
                                required property var modelData
                                width: cards.width
                                item: modelData
                                selected: modelData.code === board.selected
                                onClicked: board.picked(modelData.code)
                            }

                            Label {
                                anchors.centerIn: parent
                                visible: cards.count === 0
                                text: board.filter ? qsTr("No matches") : qsTr("Nothing waiting")
                                color: "#5c5c66"
                            }

                            // Keeps the picked card in view as J and K move.
                            Connections {
                                target: board
                                function onSelectedChanged() {
                                    for (let i = 0; i < column.modelData.items.length; ++i)
                                        if (column.modelData.items[i].code === board.selected) {
                                            cards.positionViewAtIndex(i, ListView.Contain)
                                            if (column.x < flick.contentX)
                                                flick.contentX = column.x
                                            else if (column.x + column.width > flick.contentX + flick.width)
                                                flick.contentX = column.x + column.width - flick.width
                                        }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    Label {
        anchors.centerIn: parent
        visible: backend.parties.length === 0
        text: qsTr("Add the project team with T.")
        color: "#8a8a94"
    }
}
