import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs

import QtCore

import SortFilterProxyModel 0.2

import PageEnum 1.0
import ContainerProps 1.0
import Style 1.0

import "./"
import "../Controls2"
import "../Controls2/TextTypes"
import "../Config"
import "../Components"

PageType {
    id: root

    readonly property bool isWindows: Qt.platform.os === "windows"
    property bool pageEnabled

    Component.onCompleted: {
        if (root.isWindows) {
            root.pageEnabled = true
            AppSplitTunnelingController.scanFolders()
        } else if (ConnectionController.isConnected) {
            PageController.showNotificationMessage(qsTr("Cannot change split tunneling settings during active connection"))
            root.pageEnabled = false
        } else {
            root.pageEnabled = true
        }
    }

    Connections {
        target: AppSplitTunnelingController

        function onFinished(message) {
            PageController.showNotificationMessage(message)
        }

        function onErrorOccurred(errorMessage) {
            PageController.showErrorMessage(errorMessage)
        }
    }

    readonly property var routeModes: [
        { type: 1, segmentName: qsTr("Via VPN"), name: qsTr("Only the apps from the list should have access via VPN") },
        { type: 2, segmentName: qsTr("Directly"), name: qsTr("Apps from the list should not have access via VPN") }
    ]

    readonly property int routeModeIndex: AppSplitTunnelingController.routeMode === 2 ? 1 : 0

    function folderSubtitle(path, executablesCount) {
        if (executablesCount === -1) {
            return qsTr("Searching for .exe files… · %1").arg(path)
        }
        if (executablesCount >= AppSplitTunnelingController.maxFolderExecutables) {
            return qsTr("Too many .exe files, only the first %1 bypass the VPN · %2")
                .arg(AppSplitTunnelingController.maxFolderExecutables).arg(path)
        }
        if (executablesCount >= 0) {
            return qsTr(".exe files: %1 · %2").arg(executablesCount).arg(path)
        }
        return qsTr("Folder: %1").arg(path)
    }

    function askRemoveExclusion(name, sourceIndex) {
        Dialogs.askQuestion(qsTr("Remove %1 from exclusions?").arg(name), "", qsTr("Remove exclusion"), qsTr("Cancel"),
                            function() { AppSplitTunnelingController.removeApp(sourceIndex) }, function() {})
    }

    ListViewType {
        id: listView

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: addAppPanel.top

        model: SortFilterProxyModel {
            id: proxyAppSplitTunnelingModel
            sourceModel: AppSplitTunnelingModel
            filters: AnyOf {
                RegExpFilter {
                    roleName: "appPath"
                    pattern: ".*" + GC.escapeRegExp(searchField.textField.text) + ".*"
                    caseSensitivity: Qt.CaseInsensitive
                }
                RegExpFilter {
                    roleName: "fullPath"
                    pattern: ".*" + GC.escapeRegExp(searchField.textField.text) + ".*"
                    caseSensitivity: Qt.CaseInsensitive
                }
            }
            sorters: [
                RoleSorter { roleName: "appPath"; sortOrder: Qt.AscendingOrder }
            ]
        }

        header: ColumnLayout {
            width: listView.width

            z: 2

            spacing: 0

            onHeightChanged: {
                if (!listView.moving) {
                    Qt.callLater(listView.positionViewAtBeginning)
                }
            }

            NavigationBarType {
                id: navigationBar

                Layout.fillWidth: true

                title: root.isWindows ? qsTr("App and folder exclusions") : qsTr("App split tunneling")
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                enabled: root.pageEnabled

                text: root.isWindows ? qsTr("App and folder exclusions") : qsTr("App split tunneling")

                onClicked: {
                    splitTunnelingSwitch.toggle()
                    splitTunnelingSwitch.toggled()
                }

                SwitchType {
                    id: splitTunnelingSwitch

                    focusPolicy: Qt.NoFocus

                    checked: AppSplitTunnelingController.isSplitTunnelingEnabled

                    onToggled: AppSplitTunnelingController.toggleSplitTunneling(checked)
                }
            }

            SegmentedControlType {
                id: modeSelector

                Layout.fillWidth: true
                Layout.topMargin: 24
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: !root.isWindows
                enabled: (Qt.platform.os === "android") && root.pageEnabled

                model: root.routeModes.map(function(mode) { return mode.segmentName })
                currentIndex: root.routeModeIndex

                onActivated: function(index) {
                    if (AppSplitTunnelingController.routeMode !== root.routeModes[index].type) {
                        AppSplitTunnelingController.routeMode = root.routeModes[index].type
                    }
                }

                Connections {
                    target: AppSplitTunnelingController

                    function onRouteModeChanged() {
                        modeSelector.currentIndex = root.routeModeIndex
                    }
                }
            }

            BodyTextType {
                Layout.fillWidth: true
                Layout.topMargin: 8
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 24

                text: root.isWindows ? qsTr("Apps and folders from the list bypass the VPN, everything else goes through it")
                                     : root.routeModes[modeSelector.currentIndex].name
                color: AmneziaStyle.color.textSecondary
            }

            NotificationType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8

                visible: root.isWindows && ConnectionController.isConnected

                variant: NotificationType.Warning
                text: ConnectionController.isSplitTunnelFailed
                      ? qsTr("App exclusions are not working. Reconnect the VPN to apply them")
                      : qsTr("The VPN is connected. Changes will apply after reconnecting the VPN")
            }

            ButtonType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                visible: root.isWindows && ConnectionController.isConnected
                         && (ConnectionController.isReconnectNeeded || ConnectionController.isSplitTunnelFailed)

                text: qsTr("Reconnect")

                onClicked: ConnectionController.reconnect()
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: root.isWindows

                text: qsTr("Add app")
                subtitle: qsTr("Choose .exe files")
                trailingIconSource: "qrc:/images/controls/plus.svg"

                onClicked: appFileDialog.open()
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16

                visible: root.isWindows

                text: qsTr("Add running program")
                subtitle: qsTr("Choose from the programs running now, for example a game")
                trailingIconSource: "qrc:/images/controls/plus.svg"

                onClicked: runningAppsDrawer.open()
            }

            ListItemType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 8

                visible: root.isWindows

                text: qsTr("Add folder")
                subtitle: qsTr("All .exe files in the folder and its subfolders will bypass the VPN")
                trailingIconSource: "qrc:/images/controls/plus.svg"

                onClicked: folderDialog.open()
            }

            BodyTextType {
                Layout.fillWidth: true
                Layout.leftMargin: 16
                Layout.rightMargin: 16
                Layout.bottomMargin: 16

                visible: root.isWindows

                text: qsTr("Folders are checked on every connection, so new .exe files in them bypass the VPN too. Programs that were already running when the VPN connected need a restart. For games, add the launcher too (Steam, Epic Games and others): games started from it will also bypass the VPN")
                color: AmneziaStyle.color.textSecondary
            }
        }

        delegate: Item {
            width: listView.width
            implicitHeight: appItem.implicitHeight

            ListItemType {
                id: appItem

                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16

                text: appPath
                subtitle: isFolder ? root.folderSubtitle(fullPath, executablesCount) : fullPath
                trailingIconSource: isFolder ? "qrc:/images/controls/chevron-right.svg" : "qrc:/images/controls/trash.svg"

                onClicked: {
                    const sourceIndex = proxyAppSplitTunnelingModel.mapToSource(index)
                    if (isFolder) {
                        folderExecutablesDrawer.folderName = appPath
                        folderExecutablesDrawer.folderPath = fullPath
                        folderExecutablesDrawer.sourceIndex = sourceIndex
                        folderExecutablesDrawer.open()
                    } else {
                        root.askRemoveExclusion(appPath, sourceIndex)
                    }
                }
            }
        }

        footer: EmptyStateType {
            width: listView.width
            height: visible ? implicitHeight : 0

            visible: listView.count === 0

            iconSource: searchField.textField.text !== "" ? "qrc:/images/controls/search.svg" : ""
            title: searchField.textField.text !== "" ? qsTr("Nothing found") : qsTr("No exclusions yet")
            description: searchField.textField.text === "" && root.isWindows
                         ? qsTr("Add an app, a running program or a folder above") : ""
        }
    }

    Rectangle {
        id: addAppPanel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.bottomMargin: searchField.textField.activeFocus ? 0 : PageController.imeHeight

        implicitHeight: searchField.implicitHeight + 32 + PageController.safeAreaBottomMargin

        color: AmneziaStyle.color.bgBase

        TextFieldType {
            id: searchField

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 24
            anchors.leftMargin: 16
            anchors.rightMargin: 16

            enabled: root.pageEnabled

            textField.placeholderText: root.isWindows ? qsTr("Search exclusions") : qsTr("application name")
            buttonImageSource: root.isWindows ? "" : "qrc:/images/controls/plus.svg"
            buttonJoined: true

            rightButtonClickedOnEnter: !root.isWindows

            clickedFunc: function() {
                searchField.focus = false
                PageController.showBusyIndicator(true)

                if (Qt.platform.os === "android") {
                    installedAppDrawer.open()
                }

                PageController.showBusyIndicator(false)
            }
        }
    }

    InstalledAppsDrawer {
        id: installedAppDrawer
    }

    RunningAppsDrawer {
        id: runningAppsDrawer
    }

    FolderExecutablesDrawer {
        id: folderExecutablesDrawer

        property int sourceIndex: -1

        onRemoveRequested: {
            close()
            root.askRemoveExclusion(folderName, sourceIndex)
        }
    }

    FileDialog {
        id: appFileDialog

        fileMode: FileDialog.OpenFiles
        acceptLabel: qsTr("Open executable file")
        nameFilters: [ qsTr("Executable files (*.exe)") ]

        onAccepted: AppSplitTunnelingController.addAppFiles(selectedFiles)
    }

    FolderDialog {
        id: folderDialog

        onAccepted: AppSplitTunnelingController.addFolder(selectedFolder)
    }
}
