import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"

import SortFilterProxyModel 0.2

import RunningAppsModel 1.0
import Style 1.0

SheetType {
    id: root

    expandedHeight: windowHeight * 0.9

    onAboutToShow: {
        searchField.text = ""
        runningAppsModel.updateModel()
    }

    RunningAppsModel {
        id: runningAppsModel
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

            headerText: qsTr("Running programs")
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

            model: SortFilterProxyModel {
                id: proxyRunningAppsModel
                sourceModel: runningAppsModel
                filters: RegExpFilter {
                    roleName: "appPath"
                    pattern: ".*" + GC.escapeRegExp(searchField.text) + ".*"
                    caseSensitivity: Qt.CaseInsensitive
                }
            }

            delegate: Item {
                width: listView.width - listView.leftMargin - listView.rightMargin
                implicitHeight: checkBox.implicitHeight + 16

                CheckBoxType {
                    id: checkBox

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    anchors.leftMargin: 16
                    anchors.rightMargin: 12

                    text: appName
                    descriptionText: appPath
                    checked: isAppSelected
                    onToggled: {
                        runningAppsModel.selectedStateChanged(proxyRunningAppsModel.mapToSource(index), checked)
                    }
                }

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom

                    height: 1
                    color: AmneziaStyle.color.borderSoft
                }
            }

            EmptyStateType {
                anchors.fill: parent

                visible: listView.count === 0
                title: qsTr("Nothing found")
            }
        }

        SearchFieldType {
            id: searchField

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: addButton.top
            anchors.bottomMargin: 12
            anchors.rightMargin: 16
            anchors.leftMargin: 16

            placeholderText: qsTr("program name or path")
        }

        ButtonType {
            id: addButton

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: addFoldersButton.top
            anchors.bottomMargin: 8
            anchors.rightMargin: 16
            anchors.leftMargin: 16

            text: qsTr("Add selected")

            onClicked: {
                AppSplitTunnelingController.addAppPaths(runningAppsModel.getSelectedAppPaths())
                root.close()
            }
        }

        ButtonType {
            id: addFoldersButton

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.bottomMargin: Math.max(16, PageController.safeAreaBottomMargin)
            anchors.rightMargin: 16
            anchors.leftMargin: 16

            variant: ButtonType.Secondary
            text: qsTr("Add their folders")

            onClicked: {
                AppSplitTunnelingController.addAppFolders(runningAppsModel.getSelectedAppPaths())
                root.close()
            }
        }
    }
}
