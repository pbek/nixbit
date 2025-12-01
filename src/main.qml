import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "NixOS Updater"
    width: settingsManager ? settingsManager.windowWidth : 1000
    height: settingsManager ? settingsManager.windowHeight : 800
    x: settingsManager && settingsManager.windowX >= 0 ? settingsManager.windowX : Screen.width / 2 - width / 2
    y: settingsManager && settingsManager.windowY >= 0 ? settingsManager.windowY : Screen.height / 2 - height / 2

    // Save window size when it changes
    onWidthChanged: {
        if (settingsManager && width > 0) {
            settingsManager.windowWidth = width;
        }
    }

    onHeightChanged: {
        if (settingsManager && height > 0) {
            settingsManager.windowHeight = height;
        }
    }

    // Save window position when it changes
    onXChanged: {
        if (settingsManager && x >= 0) {
            settingsManager.windowX = x;
        }
    }

    onYChanged: {
        if (settingsManager && y >= 0) {
            settingsManager.windowY = y;
        }
    }

    // Fetch updates and generations when window becomes visible
    onVisibilityChanged: {
        if (visibility === Window.Windowed || visibility === Window.Maximized || visibility === Window.FullScreen) {
            // Window is now visible, refresh data
            console.log("Window became visible, fetching updates and generations");

            // Check for updates if not already busy
            if (gitManager && !gitManager.isBusy) {
                gitManager.checkForUpdates();
            }

            // Refresh generations if not already loading
            if (generationManager && !generationManager.isLoading) {
                generationManager.loadGenerations();
            }
        }
    }

    // Confirmation dialog for deleting local repository
    Kirigami.PromptDialog {
        id: deleteConfirmDialog
        title: "Delete Local Repository"
        subtitle: "Are you sure you want to delete the local repository at:\n" + (gitManager ? gitManager.localPath : "")
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel

        onAccepted: {
            if (!gitManager)
                return;
            var path = gitManager.localPath;

            // Validate the path before deletion
            if (!path || path === "") {
                messageBox.text = "Error: Local path is empty.";
                messageBox.type = Kirigami.MessageType.Error;
                messageBox.visible = true;
                return;
            }

            // Prevent deletion of dangerous paths
            var dangerousPaths = ["/", "/home", "/root", "/usr", "/etc", "/var", "/bin", "/sbin", "/lib", "/lib64", "/opt", "/boot", "/dev", "/proc", "/sys"];
            if (dangerousPaths.indexOf(path) !== -1) {
                messageBox.text = "Error: Cannot delete system directory: " + path;
                messageBox.type = Kirigami.MessageType.Error;
                messageBox.visible = true;
                return;
            }

            // Additional check: path should contain reasonable depth (not too shallow)
            var pathParts = path.split('/').filter(function (part) {
                return part !== '';
            });

            // Prevent deletion of home directory (e.g., /home/username)
            if (pathParts.length < 3 || (pathParts[0] === 'home' && pathParts.length === 2) || (pathParts[0] === 'root' && pathParts.length === 1)) {
                messageBox.text = "Error: Path appears too shallow to be a repository path: " + path;
                messageBox.type = Kirigami.MessageType.Error;
                messageBox.visible = true;
                return;
            }

            // Execute the deletion command
            var deleteCmd = "rm -rf '" + path + "'";
            var args = ["-c", deleteCmd];
            processManager.runCommand("bash", args);
            messageBox.text = "Deleting local repository...";
            messageBox.type = Kirigami.MessageType.Information;
            messageBox.visible = true;
        }
    }

    // Confirmation dialog for changing repository URL
    Kirigami.PromptDialog {
        id: changeUrlConfirmDialog
        title: "Change Repository URL"
        subtitle: "Changing the repository URL will delete the current local repository and clone the new one.\n\nCurrent repository: " + (gitManager ? gitManager.repositoryUrl : "") + "\nNew repository: " + newUrl + "\n\nContinue?"
        standardButtons: Kirigami.Dialog.Ok | Kirigami.Dialog.Cancel
        property string newUrl: ""

        onAccepted: {
            if (!gitManager || !processManager)
                return;

            var path = gitManager.localPath;

            // Validate the path before deletion
            if (path && path !== "") {
                // Execute the deletion command
                var deleteCmd = "rm -rf '" + path + "'";
                processManager.runCommand("bash", ["-c", deleteCmd]);
            }

            // Set new URL and clone
            gitManager.repositoryUrl = newUrl;
            gitManager.cloneOrPullRepository();
        }
    }

    SettingsDialog {
        id: settingsDialog
    }

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "&Quit"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: "&Tools"
            Action {
                text: "&Check for Updates"
                enabled: gitManager ? !gitManager.isBusy : false
                onTriggered: {
                    if (gitManager)
                        gitManager.checkForUpdates();
                }
            }
            Action {
                text: "&Settings"
                onTriggered: {
                    settingsDialog.open();
                }
            }
        }
    }

    pageStack.initialPage: Kirigami.Page {
        title: (typeof isDebugMode !== 'undefined' && isDebugMode) ? "System Configuration (Debug Mode)" : "System Configuration"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            // Repository Configuration Section
            Kirigami.FormLayout {
                Layout.fillWidth: true

                TextField {
                    id: repoUrlField
                    Kirigami.FormData.label: "Repository URL:"
                    text: gitManager ? gitManager.repositoryUrl : ""
                    placeholderText: "https://github.com/user/repo.git"
                    enabled: gitManager ? (!gitManager.isBusy && !gitManager.isUrlFromGlobalSettings) : false
                    ToolTip.visible: hovered && gitManager && gitManager.isUrlFromGlobalSettings
                    ToolTip.text: "Repository URL is set by global settings and cannot be changed"
                    onEditingFinished: {
                        if (gitManager && text !== gitManager.repositoryUrl) {
                            // Open confirmation dialog before changing URL
                            changeUrlConfirmDialog.newUrl = text;
                            changeUrlConfirmDialog.open();
                        }
                    }
                }

                TextField {
                    id: hostnameField
                    Kirigami.FormData.label: "Hostname:"
                    text: settingsManager ? settingsManager.hostname : ""
                    placeholderText: "System hostname for NixOS rebuild"
                    enabled: (gitManager ? !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                    onEditingFinished: {
                        if (settingsManager)
                            settingsManager.hostname = text;
                    }
                }

                ComboBox {
                    id: rebuildModeComboBox
                    Kirigami.FormData.label: "Rebuild Mode:"
                    model: ["build", "switch"]
                    currentIndex: (typeof isDebugMode !== 'undefined' && isDebugMode) ? 0 : 1
                    enabled: (gitManager ? !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                    ToolTip.visible: hovered
                    ToolTip.text: currentIndex === 0 ? "Build the system without activating (no sudo required)" : "Build and activate the new system (requires sudo)"
                    onCurrentTextChanged: {
                        // Restore build host selector when mode text changes
                        console.log("Mode changed to:", currentText);
                        buildHostComboBox.restoreSelectionForMode(currentText);
                    }
                }

                Label {
                    text: rebuildModeComboBox.currentText === "build" ? "• Build: Tests the configuration without applying changes (no sudo required)" : "• Switch: Builds and activates the new configuration (requires sudo)"
                    font.italic: true
                    font.pixelSize: 12
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }

                ComboBox {
                    id: buildHostComboBox
                    Kirigami.FormData.label: "Build Host:"
                    property var hostList: {
                        var hosts = ["(local)"];
                        if (settingsManager && settingsManager.buildHosts) {
                            hosts = hosts.concat(settingsManager.buildHosts);
                        }
                        return hosts;
                    }
                    property bool ignoreChanges: false
                    property string currentMode: rebuildModeComboBox.currentText
                    model: hostList
                    enabled: (gitManager ? !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                    ToolTip.visible: hovered
                    ToolTip.text: "Select which host to use for building. Each mode remembers its own selection."

                    Component.onCompleted: {
                        restoreSelectionForMode(rebuildModeComboBox.currentText);
                    }

                    onCurrentIndexChanged: {
                        // Save immediately when selection changes
                        if (ignoreChanges || !settingsManager) {
                            console.log("Ignoring change, ignoreChanges:", ignoreChanges);
                            return;
                        }

                        var selectedHost = currentIndex === 0 ? "" : hostList[currentIndex];
                        console.log("Saving selection for mode:", currentMode, "host:", selectedHost);

                        if (currentMode === "build") {
                            settingsManager.selectedBuildHost = selectedHost;
                        } else {
                            settingsManager.selectedSwitchHost = selectedHost;
                        }
                    }

                    function restoreSelectionForMode(mode) {
                        if (!settingsManager) {
                            console.log("No settingsManager");
                            return;
                        }

                        ignoreChanges = true;
                        currentMode = mode;

                        var selectedHost = mode === "build" ? settingsManager.selectedBuildHost : settingsManager.selectedSwitchHost;
                        console.log("Restoring selection for mode:", mode, "saved host:", selectedHost);

                        if (selectedHost === "" || selectedHost === "(local)") {
                            currentIndex = 0;
                        } else {
                            var found = false;
                            for (var i = 0; i < hostList.length; i++) {
                                if (hostList[i] === selectedHost) {
                                    currentIndex = i;
                                    found = true;
                                    console.log("Found host at index:", i);
                                    break;
                                }
                            }
                            // If previously selected host not found, default to local
                            if (!found) {
                                console.log("Host not found, defaulting to local");
                                currentIndex = 0;
                            }
                        }

                        // Small delay before re-enabling changes to ensure index is fully set
                        Qt.callLater(function () {
                            ignoreChanges = false;
                        });
                    }

                    Connections {
                        target: settingsManager
                        function onBuildHostsChanged() {
                            console.log("Build hosts changed, updating list");
                            buildHostComboBox.hostList = (function () {
                                    var hosts = ["(local)"];
                                    if (settingsManager && settingsManager.buildHosts) {
                                        hosts = hosts.concat(settingsManager.buildHosts);
                                    }
                                    return hosts;
                                })();
                            buildHostComboBox.restoreSelectionForMode(rebuildModeComboBox.currentText);
                        }
                    }
                }

                Label {
                    Kirigami.FormData.label: "Status:"
                    text: gitManager ? gitManager.status : ""
                    font.bold: true
                    color: gitManager ? (gitManager.isBusy ? Kirigami.Theme.activeTextColor : Kirigami.Theme.positiveTextColor) : Kirigami.Theme.positiveTextColor
                }

                RowLayout {
                    Kirigami.FormData.label: "Commits Behind:"
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        text: gitManager ? (gitManager.commitsBehind >= 0 ? gitManager.commitsBehind.toString() : "N/A") : "N/A"
                        font.bold: gitManager ? gitManager.commitsBehind > 0 : false
                        color: gitManager ? (gitManager.commitsBehind > 0 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor) : Kirigami.Theme.positiveTextColor
                    }

                    Button {
                        text: "View Commits"
                        icon.name: "view-list-details"
                        visible: gitManager && gitManager.commitsBehind > 0
                        onClicked: commitsDialog.open()
                    }
                }
            }

            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Button {
                    text: "Update System"
                    icon.name: "system-software-update"
                    enabled: (processManager ? !processManager.isRunning : false) && (gitManager ? !gitManager.isBusy : false)
                    onClicked: {
                        // First pull the repository, then update system
                        if (gitManager)
                            gitManager.pullRepository();
                    }
                    Layout.fillWidth: true
                }

                Button {
                    text: "Check for Updates"
                    icon.name: "view-refresh"
                    enabled: gitManager ? !gitManager.isBusy : false
                    onClicked: {
                        if (gitManager)
                            gitManager.checkForUpdates();
                    }
                    Layout.fillWidth: true
                }
            }

            // Status Messages
            Kirigami.InlineMessage {
                id: messageBox
                Layout.fillWidth: true
                visible: false
                showCloseButton: true

                onVisibleChanged: {
                    if (visible) {
                        messageTimer.restart();
                    }
                }

                Timer {
                    id: messageTimer
                    interval: 5000
                    onTriggered: messageBox.visible = false
                }
            }

            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: gitManager ? gitManager.isBusy : false
                visible: gitManager ? gitManager.isBusy : false
            }

            ProgressBar {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                visible: (gitManager ? gitManager.isBusy : false) && (gitManager ? gitManager.status.indexOf("Cloning") !== -1 : false)
                from: 0
                to: 100
                value: gitManager ? gitManager.progress : 0

                Component.onCompleted: {
                    console.log("ProgressBar created");
                }

                onVisibleChanged: {
                    console.log("ProgressBar visibility changed to:", visible);
                    console.log("  - isBusy:", gitManager ? gitManager.isBusy : false);
                    console.log("  - status:", gitManager ? gitManager.status : "");
                    console.log("  - progress:", gitManager ? gitManager.progress : 0);
                }

                onValueChanged: {
                    console.log("ProgressBar value changed to:", value);
                }
            }

            // NixOS Generations Section
            GroupBox {
                title: "NixOS Generations"
                Layout.fillWidth: true
                visible: processManager ? !processManager.isRunning : true

                RowLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.largeSpacing

                    Label {
                        text: "Current Generation:"
                        font.bold: true
                    }

                    Label {
                        text: generationManager ? "#" + generationManager.currentGenerationNumber : "N/A"
                        font.pixelSize: 16
                        font.bold: true
                        color: Kirigami.Theme.positiveTextColor
                    }

                    Label {
                        text: "•"
                        color: Kirigami.Theme.disabledTextColor
                    }

                    Label {
                        text: generationManager && generationManager.currentGenerationDate ? generationManager.currentGenerationDate : "Unknown"
                        font.pixelSize: 14
                    }

                    Item {
                        Layout.fillWidth: true
                    }

                    Button {
                        icon.name: "view-refresh"
                        text: "Refresh"
                        enabled: generationManager ? !generationManager.isLoading : false
                        onClicked: {
                            if (generationManager)
                                generationManager.loadGenerations();
                        }
                    }

                    Button {
                        icon.name: "view-list-details"
                        text: "View All Generations"
                        onClicked: {
                            generationsDialog.open();
                        }
                    }
                }

                Component.onCompleted: {
                    if (generationManager)
                        generationManager.loadGenerations();
                }
            }

            // Generations Dialog
            GenerationsDialog {
                id: generationsDialog
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
            }

            // Commits Dialog
            CommitsDialog {
                id: commitsDialog
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
            }

            // Logs Dialog
            LogsDialog {
                id: logsDialog
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2
            }

            // System Resources Section
            GroupBox {
                title: "System Resources"
                Layout.fillWidth: true
                visible: systemMonitor ? systemMonitor.active : false

                RowLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    // CPU
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "CPU"
                            font.bold: true
                            // font.pointSize: 14
                        }
                        ProgressBar {
                            Layout.preferredWidth: 80
                            from: 0
                            to: 100
                            value: systemMonitor ? systemMonitor.cpuUsage : 0
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.cpuUsage.toFixed(1) + "%" : "0.0%"
                            // font.pointSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // RAM
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "RAM"
                            font.bold: true
                            // font.pointSize: 14
                        }
                        ProgressBar {
                            Layout.preferredWidth: 80
                            from: 0
                            to: 100
                            value: systemMonitor ? systemMonitor.memoryUsage : 0
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.usedMemory + " / " + systemMonitor.totalMemory : ""
                            // font.pointSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Network
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "Network"
                            font.bold: true
                            // font.pointSize: 14
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.networkStats : "↓ 0 B/s ↑ 0 B/s"
                            // font.pointSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Disk I/O
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "Disk I/O"
                            font.bold: true
                            // font.pointSize: 14
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.diskStats : "↓ 0 B/s ↑ 0 B/s"
                            // font.pointSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Load
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "Load"
                            font.bold: true
                            // font.pointSize: 14
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.systemLoad.toFixed(2) : "0.00"
                            // font.pointSize: 14
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Terminal Output Section
            GroupBox {
                title: "Terminal Output"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    // Build Result Status Message
                    Kirigami.InlineMessage {
                        id: buildResultMessage
                        Layout.fillWidth: true
                        visible: processManager && processManager.hasFinished && !processManager.isRunning
                        type: processManager && processManager.lastExitCode === 0 ? Kirigami.MessageType.Positive : Kirigami.MessageType.Error
                        text: {
                            if (!processManager || !processManager.hasFinished) {
                                return "";
                            }
                            if (processManager.lastExitCode === 0) {
                                return "✓ Build completed successfully!";
                            } else {
                                return "✗ Build failed with exit code " + processManager.lastExitCode;
                            }
                        }
                        showCloseButton: true
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#0d1117"
                        border.color: "#30363d"
                        border.width: 1
                        radius: 4

                        ScrollView {
                            id: terminalScrollView
                            anchors.fill: parent
                            anchors.margins: 5
                            clip: true
                            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                            Flickable {
                                id: terminalFlickable
                                contentWidth: terminalText.width
                                contentHeight: terminalText.height
                                boundsBehavior: Flickable.StopAtBounds

                                function scrollToBottom() {
                                    if (contentHeight > height) {
                                        contentY = contentHeight - height;
                                    }
                                }

                                TextEdit {
                                    id: terminalText
                                    width: terminalScrollView.availableWidth
                                    textFormat: TextEdit.RichText
                                    wrapMode: TextEdit.Wrap
                                    font.family: "Monospace"
                                    font.pixelSize: 14
                                    color: "#c9d1d9"
                                    readOnly: true
                                    selectByMouse: true
                                    selectByKeyboard: true

                                    property string cachedOutput: ""
                                    property string cachedHighlightedOutput: ""

                                    text: {
                                        if (!processManager || !processManager.output) {
                                            cachedOutput = "";
                                            cachedHighlightedOutput = "";
                                            return "";
                                        }

                                        var currentOutput = processManager.output;

                                        // Only re-highlight if output changed
                                        if (currentOutput !== cachedOutput) {
                                            cachedOutput = currentOutput;
                                            // Use C++ highlighter for better performance
                                            cachedHighlightedOutput = outputHighlighter ? outputHighlighter.highlight(currentOutput) : currentOutput;
                                        }

                                        return cachedHighlightedOutput;
                                    }

                                    onTextChanged: {
                                        Qt.callLater(terminalFlickable.scrollToBottom);
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        acceptedButtons: Qt.RightButton
                                        onClicked: {
                                            contextMenu.popup();
                                        }
                                    }

                                    Menu {
                                        id: contextMenu

                                        MenuItem {
                                            text: "Copy"
                                            icon.name: "edit-copy"
                                            enabled: terminalText.selectedText.length > 0
                                            onTriggered: terminalText.copy()
                                        }

                                        MenuItem {
                                            text: "Select All"
                                            icon.name: "edit-select-all"
                                            onTriggered: terminalText.selectAll()
                                        }

                                        MenuSeparator {}

                                        MenuItem {
                                            text: "Deselect"
                                            enabled: terminalText.selectedText.length > 0
                                            onTriggered: terminalText.deselect()
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: processManager ? (processManager.isRunning ? "Process running..." : "Ready") : "Ready"
                            color: processManager ? (processManager.isRunning ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor) : Kirigami.Theme.positiveTextColor
                            font.family: "Monospace"
                            font.pixelSize: 14
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Clear"
                            enabled: processManager ? !processManager.isRunning : false
                            onClicked: {
                                if (processManager)
                                    processManager.clearOutput();
                            }
                        }

                        Button {
                            text: "Kill Process"
                            enabled: processManager ? processManager.isRunning : false
                            onClicked: {
                                if (processManager)
                                    processManager.killProcess();
                            }
                        }

                        Button {
                            text: processManager ? (processManager.isPaused ? "Resume" : "Pause") : "Pause"
                            enabled: processManager ? processManager.isRunning : false
                            visible: rebuildModeComboBox.currentText !== "switch"
                            onClicked: {
                                if (processManager) {
                                    if (processManager.isPaused) {
                                        processManager.resumeProcess();
                                    } else {
                                        processManager.pauseProcess();
                                    }
                                }
                            }
                        }

                        Button {
                            icon.name: "documentation"
                            text: "View Logs"
                            onClicked: {
                                logsDialog.open();
                            }
                        }
                    }
                }
            }
        }
    }

    Connections {
        target: gitManager

        function onOperationCompleted(success, message) {
            messageBox.text = message;
            messageBox.type = success ? Kirigami.MessageType.Positive : Kirigami.MessageType.Error;
            messageBox.visible = true;
        }

        function onPullCompletedForUpdate() {
            // Pull completed successfully, now run system update
            if (!settingsManager || !gitManager || !processManager)
                return;
            var hostname = settingsManager.hostname;
            var repoPath = gitManager.localPath;
            var rebuildMode = rebuildModeComboBox.currentText;

            // Get the selected build host based on the current mode
            var selectedHost = rebuildMode === "build" ? settingsManager.selectedBuildHost : settingsManager.selectedSwitchHost;
            var buildHost = "";

            // If a host is selected, get its address
            if (selectedHost && selectedHost !== "" && selectedHost !== "(local)") {
                buildHost = settingsManager.getBuildHostAddress(selectedHost);
            }

            // Sanitize hostname to prevent command injection
            // Only allow alphanumeric characters, hyphens, underscores, and dots
            var sanitizedHostname = hostname.replace(/[^a-zA-Z0-9._-]/g, '');
            if (sanitizedHostname === "" || sanitizedHostname !== hostname) {
                messageBox.text = "Error: Invalid hostname. Only alphanumeric characters, hyphens, underscores, and dots are allowed.";
                messageBox.type = Kirigami.MessageType.Error;
                messageBox.visible = true;
                return;
            }

            // Sanitize build host to prevent command injection
            // Allow alphanumeric, hyphens, underscores, dots, and @ for user@host format
            var sanitizedBuildHost = "";
            if (buildHost && buildHost.trim() !== "") {
                sanitizedBuildHost = buildHost.trim().replace(/[^a-zA-Z0-9._@-]/g, '');
                if (sanitizedBuildHost === "" || sanitizedBuildHost !== buildHost.trim()) {
                    messageBox.text = "Error: Invalid build host. Only alphanumeric characters, hyphens, underscores, dots, and @ are allowed.";
                    messageBox.type = Kirigami.MessageType.Error;
                    messageBox.visible = true;
                    return;
                }
            }

            // Build the nixos-rebuild command with optional --build-host parameter
            var buildHostParam = sanitizedBuildHost !== "" ? " --build-host " + sanitizedBuildHost : "";

            var cmd;
            if (rebuildMode === "switch") {
                // Switch mode: copy to temp repo and use pkexec for sudo
                var tempScript = "/tmp/nixbit-rebuild-" + Date.now() + ".sh";
                cmd = "printf '#!/usr/bin/env bash\\n" + "set -e\\n" + "TEMP_REPO=/tmp/nixbit-repo-$$\\n" + "echo \\\"Copying repository to temporary location...\\\"\\n" + "cp -r " + repoPath + " $TEMP_REPO\\n" + "cd $TEMP_REPO\\n" + "env TERM=dumb nixos-rebuild " + rebuildMode + " --flake .#" + sanitizedHostname + buildHostParam + " -L\\n" + "echo \\\"Cleaning up temporary repository...\\\"\\n" + "rm -rf $TEMP_REPO\\n' > " + tempScript + " && chmod +x " + tempScript;
                cmd += " && pkexec " + tempScript + " ; rm -f " + tempScript;
            } else {
                // Build mode: run directly from repo (no temp copy, no temp script, no sudo)
                cmd = "cd " + repoPath + " && env TERM=dumb nixos-rebuild " + rebuildMode + " --flake .#" + sanitizedHostname + buildHostParam + " -L";
            }

            processManager.runCommand("bash", ["-c", cmd]);
        }
    }

    Connections {
        target: processManager

        function onIsRunningChanged() {
            if (systemMonitor) {
                systemMonitor.active = processManager.isRunning;
            }
            // Refresh generations when process completes (in case a new generation was created)
            // Only refresh after switch mode since build mode doesn't create a new generation
            if (!processManager.isRunning && generationManager && rebuildModeComboBox.currentText === "switch") {
                generationManager.loadGenerations();
            }
        }

        function onCommandFinished(exitCode, output) {
            // Save build log when command finishes
            if (logManager && settingsManager && output.length > 0) {
                var logDir = settingsManager.getLogDirectory();
                var maxLogs = settingsManager.maxStoredLogs;
                logManager.saveLog(output, exitCode, logDir, maxLogs);
            }
        }
    }
}
