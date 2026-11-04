import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

import Style 1.0

SheetType {
    id: root

    property string folderName
    property string folderPath
    property var executables: []
    property bool isScanning: false

    signal removeRequested()

    expandedHeight: windowHeight * 0.9

    function updateExecutables() {
        root.executables = AppSplitTunnelingController.folderExecutables(root.folderPath)
        root.isScanning = AppSplitTunnelingController.isFolderScanning(root.folderPath)
    }

    onAboutToShow: {
        searchField.text = ""
        root.updateExecutables()
    }

    Connections {
        target: AppSplitTunnelingController

        function onFolderScanned() {
            if (root.visible) {
                root.updateExecutables()
            }
        }
    }

    Item {
        id: container
        anchors.fill: parent

        SectionHeaderType {
            id: header

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 12
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            headerText: root.folderName
            descriptionText: qsTr("%1\n.exe files that bypass the VPN: %2").arg(root.folderPath).arg(root.executables.length)
        }

        ListViewType {
            id: listView

            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: searchField.top
            anchors.topMargin: 16
            anchors.bottomMargin: 8

            leftMargin: 16
            rightMargin: 16

            model: {
                const searchText = searchField.text.toLowerCase()
                return root.executables.filter(function(path) {
                    return path.toLowerCase().indexOf(searchText) !== -1
                })
            }

            delegate: ListItemType {
                width: listView.width - listView.leftMargin - listView.rightMargin

                interactive: false

                text: modelData.substring(modelData.lastIndexOf("\\") + 1)
                subtitle: modelData
            }

            EmptyStateType {
                anchors.fill: parent

                visible: listView.count === 0
                title: {
                    if (searchField.text !== "") {
                        return qsTr("Nothing found")
                    }
                    if (root.isScanning) {
                        return qsTr("Searching for .exe files…")
                    }
                    return qsTr("No .exe files in this folder")
                }
            }
        }

        SearchFieldType {
            id: searchField

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: removeButton.top
            anchors.bottomMargin: 12
            anchors.rightMargin: 16
            anchors.leftMargin: 16

            placeholderText: qsTr("file name or path")
        }

        ButtonType {
            id: removeButton

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(16, PageController.safeAreaBottomMargin)
            anchors.rightMargin: 16
            anchors.leftMargin: 16

            variant: ButtonType.Destructive
            text: qsTr("Remove from exclusions")

            onClicked: root.removeRequested()
        }
    }
}
