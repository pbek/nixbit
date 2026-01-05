import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

Dialog {
    id: settingsDialog
    title: "Settings"
    width: 900
    height: 600
    modal: true
    standardButtons: Dialog.Close

    // Main content with sidebar
    SplitView {
        anchors.fill: parent
        orientation: Qt.Horizontal

        // Left sidebar with category menu
        Rectangle {
            SplitView.preferredWidth: 200
            SplitView.minimumWidth: 150
            SplitView.maximumWidth: 300
            color: Kirigami.Theme.backgroundColor

            ListView {
                id: categoryList
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                clip: true
                currentIndex: 0

                model: ListModel {
                    ListElement {
                        name: "General"
                        icon: "preferences-system"
                        page: "general"
                    }
                    ListElement {
                        name: "Performance"
                        icon: "speedometer"
                        page: "performance"
                    }
                    ListElement {
                        name: "Repository"
                        icon: "folder-sync"
                        page: "repository"
                    }
                    ListElement {
                        name: "Build Hosts"
                        icon: "network-server"
                        page: "buildhosts"
                    }
                    ListElement {
                        name: "Debug"
                        icon: "tools-report-bug"
                        page: "debug"
                    }
                }

                delegate: ItemDelegate {
                    width: ListView.view.width
                    highlighted: ListView.isCurrentItem

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Kirigami.Icon {
                            source: model.icon
                            Layout.preferredWidth: Kirigami.Units.iconSizes.smallMedium
                            Layout.preferredHeight: Kirigami.Units.iconSizes.smallMedium
                        }

                        Label {
                            text: model.name
                            Layout.fillWidth: true
                            color: highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }
                    }

                    background: Rectangle {
                        color: highlighted ? Kirigami.Theme.highlightColor : (parent.hovered ? Kirigami.Theme.hoverColor : "transparent")
                        radius: 3
                    }

                    onClicked: {
                        categoryList.currentIndex = index;
                        stackLayout.currentIndex = index;
                    }
                }
            }
        }

        // Right content area with pages
        StackLayout {
            id: stackLayout
            SplitView.fillWidth: true
            currentIndex: 0

            // General Settings Page
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "General"
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                        font.bold: true
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        CheckBox {
                            Kirigami.FormData.label: "Start Hidden:"
                            checked: settingsManager ? settingsManager.startHidden : false
                            text: "Start application hidden in system tray"
                            onToggled: {
                                if (settingsManager)
                                    settingsManager.startHidden = checked;
                            }
                        }

                        CheckBox {
                            Kirigami.FormData.label: "Autostart:"
                            checked: settingsManager ? settingsManager.autostartEnabled : false
                            text: "Launch Nixbit automatically at login"
                            onToggled: {
                                if (settingsManager)
                                    settingsManager.setAutostartEnabled(checked);
                            }
                        }

                        RowLayout {
                            Kirigami.FormData.label: "Auto-fetch Interval:"

                            SpinBox {
                                id: fetchIntervalSpinBox
                                from: 1
                                to: 60
                                value: gitManager ? gitManager.fetchIntervalMinutes : 5
                                editable: true
                                enabled: gitManager ? !gitManager.isBusy : false
                                onValueModified: {
                                    if (gitManager)
                                        gitManager.fetchIntervalMinutes = value;
                                }
                            }

                            Label {
                                text: "minutes"
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Performance Settings Page
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "Performance"
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                        font.bold: true
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        RowLayout {
                            Kirigami.FormData.label: "Max Terminal Lines:"
                            spacing: Kirigami.Units.smallSpacing

                            SpinBox {
                                id: maxLinesSpinBox
                                from: 500
                                to: 20000
                                stepSize: 500
                                value: processManager ? processManager.maxOutputLines : 5000
                                editable: true
                                onValueModified: {
                                    if (processManager)
                                        processManager.maxOutputLines = value;
                                }
                            }

                            Label {
                                text: "lines"
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        Label {
                            text: "Lower values use less memory during builds"
                            font.italic: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                        }

                        RowLayout {
                            Kirigami.FormData.label: "Max Stored Logs:"
                            spacing: Kirigami.Units.smallSpacing

                            SpinBox {
                                id: maxLogsSpinBox
                                from: 0
                                to: 100
                                stepSize: 1
                                value: settingsManager ? settingsManager.maxStoredLogs : 10
                                editable: true
                                onValueModified: {
                                    if (settingsManager)
                                        settingsManager.maxStoredLogs = value;
                                }
                            }

                            Label {
                                text: "log files"
                                color: Kirigami.Theme.disabledTextColor
                            }
                        }

                        Label {
                            text: "0 = unlimited. Old logs are automatically deleted."
                            font.italic: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Repository Settings Page
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "Repository"
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                        font.bold: true
                    }

                    Label {
                        text: "Local Path:"
                        font.bold: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Rectangle {
                            Layout.fillWidth: true
                            Layout.minimumHeight: 40
                            Layout.preferredHeight: Math.min(localPathText.contentHeight + Kirigami.Units.largeSpacing, 100)
                            color: Kirigami.Theme.alternateBackgroundColor
                            radius: 4
                            border.color: Kirigami.Theme.disabledTextColor
                            border.width: 1

                            TextEdit {
                                id: localPathText
                                anchors.fill: parent
                                anchors.margins: Kirigami.Units.smallSpacing
                                text: gitManager ? gitManager.localPath : ""
                                readOnly: true
                                selectByMouse: true
                                wrapMode: Text.Wrap
                                color: Kirigami.Theme.textColor
                            }
                        }

                        ToolButton {
                            icon.name: "edit-delete"
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: "Delete local repository"
                            enabled: (gitManager ? gitManager.localPath !== "" : false) && (gitManager ? !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                            onClicked: {
                                deleteConfirmDialog.open();
                            }
                        }

                        ToolButton {
                            icon.name: "utilities-terminal"
                            display: AbstractButton.IconOnly
                            ToolTip.visible: hovered
                            ToolTip.text: "Open terminal here"
                            enabled: gitManager ? gitManager.localPath !== "" : false
                            onClicked: {
                                if (gitManager && gitManager.localPath) {
                                    Utils.openTerminalInDirectory(gitManager.localPath, processManager);
                                }
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Build Hosts Page
            ScrollView {
                clip: true

                ColumnLayout {
                    width: parent.width - Kirigami.Units.largeSpacing * 4
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "Build Hosts"
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                        font.bold: true
                        Layout.topMargin: Kirigami.Units.largeSpacing
                    }

                    Label {
                        text: "Configure remote hosts for building NixOS configurations"
                        color: Kirigami.Theme.disabledTextColor
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        TextField {
                            id: newHostNameField
                            Layout.preferredWidth: 200
                            placeholderText: "Name"
                        }

                        TextField {
                            id: newHostAddressField
                            Layout.fillWidth: true
                            placeholderText: "user@hostname"
                        }

                        Button {
                            text: "Add"
                            icon.name: "list-add"
                            enabled: newHostNameField.text.trim() !== "" && newHostAddressField.text.trim() !== ""
                            onClicked: {
                                if (settingsManager) {
                                    settingsManager.addBuildHost(newHostNameField.text.trim(), newHostAddressField.text.trim());
                                    newHostNameField.text = "";
                                    newHostAddressField.text = "";
                                }
                            }
                        }
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredHeight: 300
                        clip: true

                        ListView {
                            id: buildHostsList
                            model: settingsManager ? settingsManager.buildHosts : []
                            spacing: Kirigami.Units.smallSpacing

                            delegate: Item {
                                width: ListView.view.width
                                height: hostRowLayout.implicitHeight + Kirigami.Units.smallSpacing

                                RowLayout {
                                    id: hostRowLayout
                                    anchors.fill: parent
                                    spacing: Kirigami.Units.smallSpacing

                                    TextField {
                                        id: hostNameEdit
                                        Layout.preferredWidth: 200
                                        text: modelData
                                        property string originalName: modelData
                                    }

                                    TextField {
                                        id: hostAddressEdit
                                        Layout.fillWidth: true
                                        text: settingsManager ? settingsManager.getBuildHostAddress(modelData) : ""
                                    }

                                    ToolButton {
                                        icon.name: "document-save"
                                        display: AbstractButton.IconOnly
                                        enabled: hostNameEdit.text.trim() !== "" && hostAddressEdit.text.trim() !== ""
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Update"
                                        onClicked: {
                                            if (settingsManager) {
                                                settingsManager.updateBuildHost(hostNameEdit.originalName, hostNameEdit.text.trim(), hostAddressEdit.text.trim());
                                                hostNameEdit.originalName = hostNameEdit.text.trim();
                                            }
                                        }
                                    }

                                    ToolButton {
                                        icon.name: "list-remove"
                                        display: AbstractButton.IconOnly
                                        ToolTip.visible: hovered
                                        ToolTip.text: "Remove"
                                        onClicked: {
                                            if (settingsManager) {
                                                settingsManager.removeBuildHost(modelData);
                                            }
                                        }
                                    }
                                }
                            }

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                                visible: buildHostsList.count === 0
                                text: "No build hosts configured"
                                explanation: "Add build hosts to build on remote machines"
                                icon.name: "network-server"
                            }
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }

            // Debug Settings Page
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: Kirigami.Units.largeSpacing * 2
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "Debug"
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.5
                        font.bold: true
                    }

                    Label {
                        text: "Advanced debugging and diagnostics tools"
                        color: Kirigami.Theme.disabledTextColor
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true

                        CheckBox {
                            Kirigami.FormData.label: "Memory Test Mode:"
                            checked: settingsManager ? settingsManager.debugMode : false
                            text: "Enable memory debugging controls"
                            onToggled: {
                                if (settingsManager)
                                    settingsManager.debugMode = checked;
                            }
                        }

                        Label {
                            text: "Shows a test button in the terminal area to generate large amounts of log output.\nUseful for testing memory usage and identifying potential OOM issues."
                            font.italic: true
                            font.pixelSize: Kirigami.Theme.smallFont.pixelSize
                            color: Kirigami.Theme.disabledTextColor
                            Layout.columnSpan: 2
                            Layout.leftMargin: Kirigami.Units.largeSpacing
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }

                    Item {
                        Layout.fillHeight: true
                    }
                }
            }
        }
    }
}
