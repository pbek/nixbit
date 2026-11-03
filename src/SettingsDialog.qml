import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

Dialog {
    id: settingsDialog
    title: "Settings"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 46, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 32, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    padding: 0
    standardButtons: Dialog.Close

    // Title and description shown at the top of every page
    component PageHeader: ColumnLayout {
        property alias title: heading.text
        property alias description: descriptionLabel.text

        Layout.fillWidth: true
        Layout.bottomMargin: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            id: heading
            level: 2
        }

        Label {
            id: descriptionLabel
            Layout.fillWidth: true
            visible: text !== ""
            wrapMode: Text.WordWrap
            color: Kirigami.Theme.disabledTextColor
        }
    }

    // Explanation shown below a form field
    component HelpText: Label {
        Layout.fillWidth: true
        Layout.maximumWidth: Kirigami.Units.gridUnit * 22
        wrapMode: Text.WordWrap
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }

    // Scrollable page container with consistent margins
    component SettingsPage: ScrollView {
        default property alias content: pageLayout.data

        contentWidth: availableWidth
        clip: true

        ColumnLayout {
            id: pageLayout
            width: parent.width
            spacing: Kirigami.Units.smallSpacing

            Item {
                implicitHeight: Kirigami.Units.largeSpacing
            }
        }
    }

    contentItem: RowLayout {
        spacing: 0

        // Left sidebar with category menu
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: Kirigami.Units.gridUnit * 11
            Kirigami.Theme.colorSet: Kirigami.Theme.Window
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor

            ListView {
                id: categoryList
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                clip: true
                currentIndex: 0
                spacing: 1
                keyNavigationEnabled: true

                onCurrentIndexChanged: stackLayout.currentIndex = currentIndex

                model: ListModel {
                    ListElement {
                        name: "General"
                        iconName: "preferences-system"
                    }
                    ListElement {
                        name: "Repository"
                        iconName: "folder-sync"
                    }
                    ListElement {
                        name: "Build Hosts"
                        iconName: "network-server"
                    }
                    ListElement {
                        name: "Notifications"
                        iconName: "preferences-desktop-notification"
                    }
                    ListElement {
                        name: "Performance"
                        iconName: "speedometer"
                    }
                    ListElement {
                        name: "Debug"
                        iconName: "tools-report-bug"
                    }
                }

                delegate: ItemDelegate {
                    required property int index
                    required property string name
                    required property string iconName

                    width: ListView.view.width
                    text: name
                    icon.name: iconName
                    highlighted: ListView.isCurrentItem
                    onClicked: categoryList.currentIndex = index
                }
            }
        }

        Kirigami.Separator {
            Layout.fillHeight: true
        }

        // Right content area with pages
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.leftMargin: Kirigami.Units.gridUnit
            Layout.rightMargin: Kirigami.Units.largeSpacing
            currentIndex: 0

            // General Settings Page
            SettingsPage {
                PageHeader {
                    title: "General"
                    description: "How Nixbit starts and how it runs system rebuilds."
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    CheckBox {
                        Kirigami.FormData.label: "Startup:"
                        checked: settingsManager ? settingsManager.autostartEnabled : false
                        text: "Launch Nixbit automatically at login"
                        onToggled: {
                            if (settingsManager)
                                settingsManager.setAutostartEnabled(checked);
                        }
                    }

                    CheckBox {
                        checked: settingsManager ? settingsManager.startHidden : false
                        text: "Start hidden in the system tray"
                        onToggled: {
                            if (settingsManager)
                                settingsManager.startHidden = checked;
                        }
                    }

                    RowLayout {
                        Kirigami.FormData.label: "Check for updates every:"
                        spacing: Kirigami.Units.smallSpacing

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
                            text: fetchIntervalSpinBox.value === 1 ? "minute" : "minutes"
                        }
                    }

                    Item {
                        Kirigami.FormData.isSection: true
                    }

                    CheckBox {
                        Kirigami.FormData.label: "Terminal:"
                        checked: settingsManager ? settingsManager.useEmbeddedTerminal : true
                        enabled: processManager ? processManager.terminalAvailable : false
                        text: "Run commands in an embedded terminal"
                        onToggled: {
                            if (settingsManager)
                                settingsManager.useEmbeddedTerminal = checked;
                        }
                    }

                    HelpText {
                        text: processManager && processManager.terminalAvailable ? "Shows colored output and progress, and lets you enter passwords directly in Nixbit." : "Not available: the Kwi QML module (org.kde.kwi.terminal) is not installed."
                    }

                    CheckBox {
                        Kirigami.FormData.label: "Rebuild tool:"
                        checked: settingsManager ? settingsManager.useNh : false
                        enabled: processManager && settingsManager ? !settingsManager.useNhOverridden && processManager.nhAvailable && processManager.terminalAvailable && settingsManager.useEmbeddedTerminal : false
                        text: "Use nh instead of nixos-rebuild"
                        onToggled: {
                            if (settingsManager)
                                settingsManager.useNh = checked;
                        }
                    }

                    HelpText {
                        text: settingsManager && settingsManager.useNhOverridden ? "This setting is managed by /etc/nixbit.conf." + (processManager && processManager.nhAvailable && processManager.terminalAvailable && settingsManager.useEmbeddedTerminal ? "" : " nh is only used when it is in PATH and the embedded terminal is turned on.") : !processManager || !processManager.nhAvailable ? "Not available: nh was not found in PATH." : !processManager.terminalAvailable || !settingsManager.useEmbeddedTerminal ? "Not available: nh needs the embedded terminal to ask for your password." : "nh builds as your user and asks for your password in the terminal when activating."
                    }
                }
            }

            // Repository Settings Page
            SettingsPage {
                PageHeader {
                    title: "Repository"
                    description: "The local clone of your NixOS configuration repository."
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    RowLayout {
                        Kirigami.FormData.label: "Local path:"
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            Layout.fillWidth: true
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                            text: gitManager && gitManager.localPath !== "" ? gitManager.localPath : "Not cloned yet"
                            color: gitManager && gitManager.localPath !== "" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                            elide: Text.ElideMiddle

                            HoverHandler {
                                id: localPathHover
                            }

                            ToolTip.visible: localPathHover.hovered && truncated
                            ToolTip.text: text
                        }

                        ToolButton {
                            icon.name: "document-open-folder"
                            display: AbstractButton.IconOnly
                            text: "Open in file manager"
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            enabled: gitManager ? gitManager.localPath !== "" : false
                            onClicked: {
                                if (gitManager && gitManager.localPath) {
                                    processManager.startDetached("xdg-open", [gitManager.localPath]);
                                }
                            }
                        }

                        ToolButton {
                            icon.name: "utilities-terminal"
                            display: AbstractButton.IconOnly
                            text: "Open terminal here"
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            enabled: gitManager ? gitManager.localPath !== "" : false
                            onClicked: {
                                if (gitManager && gitManager.localPath) {
                                    Utils.openTerminalInDirectory(gitManager.localPath, processManager);
                                }
                            }
                        }
                    }

                    Button {
                        Kirigami.FormData.label: "Reset:"
                        text: "Delete Local Repository…"
                        icon.name: "edit-delete"
                        enabled: (gitManager ? gitManager.localPath !== "" && !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                        onClicked: deleteConfirmDialog.open()
                    }

                    HelpText {
                        text: "Removes the local clone. It will be cloned again from the repository URL the next time it is needed."
                    }
                }
            }

            // Build Hosts Page
            Item {
                ColumnLayout {
                    anchors.fill: parent
                    anchors.topMargin: Kirigami.Units.largeSpacing + Kirigami.Units.smallSpacing
                    anchors.bottomMargin: Kirigami.Units.largeSpacing
                    spacing: Kirigami.Units.smallSpacing

                    PageHeader {
                        title: "Build Hosts"
                        description: "Remote machines that can build your configuration. Select a build host for each rebuild mode in the main window."
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: 3
                        columnSpacing: Kirigami.Units.smallSpacing
                        rowSpacing: Kirigami.Units.smallSpacing

                        Label {
                            text: "Name"
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Label {
                            text: "SSH address"
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Item {
                            implicitWidth: 1
                        }

                        TextField {
                            id: newHostNameField
                            Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                            placeholderText: "e.g. workstation"
                            onAccepted: addHostButton.clicked()
                        }

                        TextField {
                            id: newHostAddressField
                            Layout.fillWidth: true
                            placeholderText: "user@hostname"
                            onAccepted: addHostButton.clicked()
                        }

                        Button {
                            id: addHostButton
                            text: "Add"
                            icon.name: "list-add"
                            enabled: newHostNameField.text.trim() !== "" && newHostAddressField.text.trim() !== ""
                            onClicked: {
                                if (settingsManager && enabled) {
                                    settingsManager.addBuildHost(newHostNameField.text.trim(), newHostAddressField.text.trim());
                                    newHostNameField.text = "";
                                    newHostAddressField.text = "";
                                }
                            }
                        }
                    }

                    Kirigami.Separator {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        Layout.bottomMargin: Kirigami.Units.smallSpacing
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true

                        ListView {
                            id: buildHostsList
                            model: settingsManager ? settingsManager.buildHosts : []
                            spacing: Kirigami.Units.smallSpacing

                            delegate: RowLayout {
                                id: hostRow
                                required property string modelData
                                readonly property string address: settingsManager ? settingsManager.getBuildHostAddress(modelData) : ""
                                readonly property bool modified: hostNameEdit.text.trim() !== modelData || hostAddressEdit.text.trim() !== address

                                width: ListView.view.width
                                spacing: Kirigami.Units.smallSpacing

                                TextField {
                                    id: hostNameEdit
                                    Layout.preferredWidth: Kirigami.Units.gridUnit * 9
                                    text: hostRow.modelData
                                    onAccepted: saveHostButton.clicked()
                                }

                                TextField {
                                    id: hostAddressEdit
                                    Layout.fillWidth: true
                                    text: hostRow.address
                                    onAccepted: saveHostButton.clicked()
                                }

                                ToolButton {
                                    id: saveHostButton
                                    icon.name: "document-save"
                                    display: AbstractButton.IconOnly
                                    text: "Save changes"
                                    enabled: hostRow.modified && hostNameEdit.text.trim() !== "" && hostAddressEdit.text.trim() !== ""
                                    ToolTip.visible: hovered
                                    ToolTip.text: text
                                    onClicked: {
                                        if (settingsManager && enabled) {
                                            settingsManager.updateBuildHost(hostRow.modelData, hostNameEdit.text.trim(), hostAddressEdit.text.trim());
                                        }
                                    }
                                }

                                ToolButton {
                                    icon.name: "edit-delete-remove"
                                    display: AbstractButton.IconOnly
                                    text: "Remove build host"
                                    ToolTip.visible: hovered
                                    ToolTip.text: text
                                    onClicked: {
                                        if (settingsManager) {
                                            settingsManager.removeBuildHost(hostRow.modelData);
                                        }
                                    }
                                }
                            }

                            Kirigami.PlaceholderMessage {
                                anchors.centerIn: parent
                                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                                visible: buildHostsList.count === 0
                                text: "No build hosts configured"
                                explanation: "Add a host above to build on a remote machine."
                                icon.name: "network-server"
                            }
                        }
                    }
                }
            }

            // Notification Settings Page
            SettingsPage {
                PageHeader {
                    title: "Notifications"
                    description: "Run a command when a build, switch or boot operation finishes. The notification text is written to the command's standard input."
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    TextField {
                        id: notificationCommandField
                        Kirigami.FormData.label: "Command:"
                        Layout.fillWidth: true
                        Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                        text: settingsManager ? settingsManager.notificationCommand : ""
                        enabled: settingsManager ? !settingsManager.notificationCommandOverridden : false
                        placeholderText: "notify-send Nixbit \"$(cat)\""
                        font.family: Kirigami.Theme.fixedWidthFont.family
                        onEditingFinished: {
                            if (settingsManager)
                                settingsManager.notificationCommand = text;
                        }
                    }

                    HelpText {
                        text: settingsManager && settingsManager.notificationCommandOverridden ? "This command is managed by /etc/nixbit.conf." : "Leave empty to disable notifications. Shell syntax is supported."
                    }
                }
            }

            // Performance Settings Page
            SettingsPage {
                PageHeader {
                    title: "Performance"
                    description: "Limit how much output and how many logs Nixbit keeps."
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    RowLayout {
                        Kirigami.FormData.label: "Terminal scrollback:"
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
                        }
                    }

                    HelpText {
                        text: "Lower values use less memory during builds."
                    }

                    RowLayout {
                        Kirigami.FormData.label: "Keep build logs:"
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
                            text: maxLogsSpinBox.value === 1 ? "log" : "logs"
                        }
                    }

                    HelpText {
                        text: "Older logs are deleted automatically. Set to 0 to keep all logs."
                    }
                }
            }

            // Debug Settings Page
            SettingsPage {
                PageHeader {
                    title: "Debug"
                    description: "Advanced debugging and diagnostics tools."
                }

                Kirigami.FormLayout {
                    Layout.fillWidth: true

                    CheckBox {
                        Kirigami.FormData.label: "Memory test mode:"
                        checked: settingsManager ? settingsManager.debugMode : false
                        text: "Show memory debugging controls"
                        onToggled: {
                            if (settingsManager)
                                settingsManager.debugMode = checked;
                        }
                    }

                    HelpText {
                        text: "Adds controls below the terminal to generate large amounts of output. Useful for testing memory usage and finding out-of-memory issues."
                    }
                }
            }
        }
    }
}
