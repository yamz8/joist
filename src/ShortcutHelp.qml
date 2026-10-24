import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

// Every shortcut, shown with ?, in two groups side by side.
Rectangle {
    id: help
    color: "#cc0b0b0d"

    readonly property var groups: [
        {
            title: qsTr("Find your way"),
            shortcuts: [
                ["1", qsTr("Ball in court")],
                ["2", qsTr("RFI log")],
                ["3", qsTr("Submittal log")],
                ["J / K", qsTr("Next and previous item")],
                ["/", qsTr("Search")],
                ["H", qsTr("Show or hide closed items")],
                ["P", qsTr("Switch or start a project")],
                ["T", qsTr("Project and team")],
                ["E", qsTr("Export this view as PDF")],
                ["?", qsTr("Show or hide shortcuts")],
                ["Q", qsTr("Quit")]
            ]
        },
        {
            title: qsTr("Work an item"),
            shortcuts: [
                ["R", qsTr("New RFI")],
                ["S", qsTr("New submittal")],
                ["Space", qsTr("Pass the ball")],
                ["C", qsTr("Answer or review; reopen")],
                ["] / [", qsTr("Due a day later or earlier")],
                ["Enter", qsTr("Edit the subject")],
                ["L", qsTr("Link a Lintel sheet")],
                ["O", qsTr("Open the linked sheet")],
                ["F", qsTr("Export the item's form")],
                ["Delete", qsTr("Delete the item")],
                ["Ctrl+Z", qsTr("Undo")],
                ["Ctrl+Shift+Z", qsTr("Redo")],
                ["Esc", qsTr("Leave a field, or step back")]
            ]
        }
    ]

    TapHandler { onTapped: win.helpVisible = false }

    Pane {
        anchors.centerIn: parent
        padding: 24
        Material.roundedScale: theme.radius
        background: Rectangle { color: "#17171a"; radius: theme.radius; border.color: "#2a2a30" }

        ColumnLayout {
            spacing: 14

            Label {
                text: qsTr("Keyboard shortcuts")
                font.pixelSize: 18
                font.bold: true
            }

            RowLayout {
                spacing: 40

                Repeater {
                    model: help.groups
                    delegate: ColumnLayout {
                        required property var modelData
                        Layout.alignment: Qt.AlignTop
                        spacing: 8

                        Label {
                            text: parent.modelData.title
                            color: "#8a8a94"
                            font.pixelSize: 12
                            font.capitalization: Font.AllUppercase
                        }

                        GridLayout {
                            columns: 2
                            columnSpacing: 20
                            rowSpacing: 7

                            Repeater {
                                model: parent.parent.modelData.shortcuts
                                delegate: Label {
                                    required property var modelData
                                    required property int index
                                    Layout.row: index
                                    Layout.column: 0
                                    text: modelData[0]
                                    color: theme.accent
                                    font.family: "monospace"
                                }
                            }
                            Repeater {
                                model: parent.parent.modelData.shortcuts
                                delegate: Label {
                                    required property var modelData
                                    required property int index
                                    Layout.row: index
                                    Layout.column: 1
                                    text: modelData[1]
                                    color: "#d8d8de"
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
