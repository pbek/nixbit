import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

ApplicationWindow {
    id: root
    title: "NixOS Updater"
    width: settingsManager ? settingsManager.windowWidth : 1000
    height: settingsManager ? settingsManager.windowHeight : 800
    x: settingsManager && settingsManager.windowX >= 0 ? settingsManager.windowX : Screen.width / 2 - width / 2
    y: settingsManager && settingsManager.windowY >= 0 ? settingsManager.windowY : Screen.height / 2 - height / 2
    minimumWidth: Kirigami.Units.gridUnit * 36
    minimumHeight: Kirigami.Units.gridUnit * 26

    readonly property bool gitBusy: gitManager ? gitManager.isBusy : false
    readonly property bool processRunning: processManager ? processManager.isRunning : false
    readonly property int commitsBehind: gitManager ? gitManager.commitsBehind : -1
    readonly property string gitStatus: gitManager ? gitManager.status : "Ready"
    readonly property bool gitStatusIsError: /^(failed|error|no repository)/i.test(gitStatus) || /\bfailed\b/i.test(gitStatus)

    function rebuildModeTitle(mode) {
        return mode === "switch" ? "Switch" : mode === "boot" ? "Boot" : "Build";
    }

    // Property to track if we're in a chained build-then-switch operation
    property bool isChainedBuildSwitch: false
    property bool shouldRebuildAfterPull: false

    // Property to track the current rebuild mode (for logging purposes)
    property string currentRebuildMode: "build"
    property int compactHeightThreshold: 760
    property bool compactHeight: height < compactHeightThreshold
    property bool topSectionExpandedOverride: false
    property bool showTopSectionDetails: !compactHeight || topSectionExpandedOverride

    // Embedded Kwi terminal state (see KwiTerminalPane.qml)
    readonly property bool kwiTerminalAvailable: kwiTerminalLoader.status === Loader.Ready
    readonly property bool kwiTerminalShown: kwiTerminalAvailable && kwiTerminalLoader.item.hasSession
    readonly property bool nhUsed: processManager !== null && processManager.useNh && processManager.terminalMode && processManager.nhAvailable

    Binding {
        target: processManager
        property: "terminalAvailable"
        value: root.kwiTerminalAvailable
        when: processManager !== null
    }

    Binding {
        target: processManager
        property: "terminalMode"
        value: root.kwiTerminalAvailable && (settingsManager ? settingsManager.useEmbeddedTerminal : true)
        when: processManager !== null
    }

    Binding {
        target: processManager
        property: "useNh"
        value: settingsManager ? settingsManager.useNh : false
        when: processManager !== null
    }

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

        if (height >= compactHeightThreshold && topSectionExpandedOverride) {
            topSectionExpandedOverride = false;
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

    // Keyboard shortcut for search (Ctrl+F)
    Shortcut {
        sequence: StandardKey.Find
        onActivated: searchBar.show()
    }

    // Keyboard shortcut for Find Next (F3)
    Shortcut {
        sequence: "F3"
        onActivated: {
            if (searchBar.visible) {
                searchBar.findNext();
            } else {
                searchBar.show();
            }
        }
    }

    // Keyboard shortcut for Find Previous (Shift+F3)
    Shortcut {
        sequence: "Shift+F3"
        onActivated: {
            if (searchBar.visible) {
                searchBar.findPrevious();
            } else {
                searchBar.show();
            }
        }
    }

    // Confirmation dialog for deleting local repository
    Kirigami.PromptDialog {
        id: deleteConfirmDialog
        title: "Delete Local Repository?"
        subtitle: "The local copy of the repository will be removed from disk:\n\n" + (gitManager ? gitManager.localPath : "") + "\n\nIt will be cloned again the next time it is needed."
        dialogType: Kirigami.PromptDialog.Warning
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: "Delete"
                icon.name: "edit-delete"
                onTriggered: deleteConfirmDialog.accept()
            }
        ]

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
        title: "Change Repository URL?"
        subtitle: "The current local repository will be deleted and the new one will be cloned.\n\nCurrent: " + (gitManager ? gitManager.repositoryUrl : "") + "\nNew: " + newUrl
        dialogType: Kirigami.PromptDialog.Warning
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: "Change and Clone"
                icon.name: "folder-sync"
                onTriggered: changeUrlConfirmDialog.accept()
            }
        ]
        property string newUrl: ""

        // Revert the text field if the change is not confirmed
        onClosed: repoUrlField.text = Qt.binding(function () {
            return gitManager ? gitManager.repositoryUrl : "";
        })

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

    AboutDialog {
        id: aboutDialog
    }

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "&Quit"
                icon.name: "application-exit"
                shortcut: StandardKey.Quit
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: "&Tools"
            Action {
                text: "&Check for Updates"
                icon.name: "view-refresh"
                enabled: !root.gitBusy
                onTriggered: {
                    if (gitManager)
                        gitManager.checkForUpdates();
                }
            }
            Action {
                text: "&Generations…"
                icon.name: "view-history"
                onTriggered: generationsDialog.open()
            }
            Action {
                text: "Build &Logs…"
                icon.name: "documentation"
                onTriggered: logsDialog.open()
            }
            MenuSeparator {}
            Action {
                text: "&Settings…"
                icon.name: "configure"
                shortcut: StandardKey.Preferences
                onTriggered: {
                    settingsDialog.open();
                }
            }
            MenuSeparator {}
            Action {
                text: "&About Nixbit"
                icon.name: "help-about"
                onTriggered: {
                    aboutDialog.open();
                }
            }
        }
    }

    // Status icon for the repository state, shows a spinner while git is busy
    component GitStatusIndicator: Item {
        implicitWidth: Kirigami.Units.iconSizes.small
        implicitHeight: Kirigami.Units.iconSizes.small

        BusyIndicator {
            anchors.centerIn: parent
            width: Kirigami.Units.iconSizes.smallMedium
            height: width
            padding: 0
            running: root.gitBusy
            visible: running
        }

        Kirigami.Icon {
            anchors.fill: parent
            visible: !root.gitBusy
            source: root.gitStatusIsError ? "emblem-error" : root.commitsBehind > 0 ? "emblem-information" : "emblem-success"
        }
    }

    // Caption and value pair used in the system resources section
    component ResourceMetric: ColumnLayout {
        property alias caption: captionLabel.text
        property alias value: valueLabel.text
        property real percent: -1

        Layout.fillWidth: true
        Layout.preferredWidth: 1
        spacing: Kirigami.Units.smallSpacing

        Label {
            id: captionLabel
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        ProgressBar {
            Layout.fillWidth: true
            Layout.maximumWidth: Kirigami.Units.gridUnit * 8
            visible: parent.percent >= 0
            from: 0
            to: 100
            value: Math.max(0, parent.percent)
        }

        Label {
            id: valueLabel
            Layout.fillWidth: true
            elide: Text.ElideRight
        }
    }

    // Main content
    Item {
        anchors.fill: parent

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

            // Repository Configuration Section
            GroupBox {
                Layout.fillWidth: true
                title: "Configuration"

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    // Summary shown instead of the full form on small windows
                    RowLayout {
                        Layout.fillWidth: true
                        visible: root.compactHeight
                        spacing: Kirigami.Units.smallSpacing

                        GitStatusIndicator {}

                        Label {
                            Layout.fillWidth: true
                            text: root.gitStatus
                            elide: Text.ElideRight
                        }

                        Label {
                            text: root.rebuildModeTitle(rebuildModeComboBox.currentText)
                            color: Kirigami.Theme.disabledTextColor
                        }

                        ToolButton {
                            text: root.showTopSectionDetails ? "Hide Details" : "Show Details"
                            icon.name: root.showTopSectionDetails ? "go-up" : "go-down"
                            onClicked: root.topSectionExpandedOverride = !root.showTopSectionDetails
                        }
                    }

                    Kirigami.FormLayout {
                        Layout.fillWidth: true
                        visible: root.showTopSectionDetails

                        RowLayout {
                            Kirigami.FormData.label: "Repository URL:"
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: repoUrlField
                                Layout.fillWidth: true
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 16
                                text: gitManager ? gitManager.repositoryUrl : ""
                                placeholderText: "https://github.com/user/repo.git"
                                enabled: gitManager ? (!gitManager.isBusy && !gitManager.isUrlFromGlobalSettings) : false
                                onEditingFinished: {
                                    if (gitManager && text !== gitManager.repositoryUrl) {
                                        // Open confirmation dialog before changing URL
                                        changeUrlConfirmDialog.newUrl = text;
                                        changeUrlConfirmDialog.open();
                                    }
                                }
                            }

                            ToolButton {
                                icon.name: "document-open-folder"
                                display: AbstractButton.IconOnly
                                text: "Open repository in file manager"
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                enabled: gitManager && gitManager.localPath !== ""
                                onClicked: {
                                    if (gitManager && gitManager.localPath) {
                                        processManager.startDetached("xdg-open", [gitManager.localPath]);
                                    }
                                }
                            }

                            ToolButton {
                                icon.name: "utilities-terminal"
                                display: AbstractButton.IconOnly
                                text: "Open terminal in repository"
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                enabled: gitManager && gitManager.localPath !== ""
                                onClicked: {
                                    if (gitManager && gitManager.localPath) {
                                        Utils.openTerminalInDirectory(gitManager.localPath, processManager);
                                    }
                                }
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            visible: gitManager ? gitManager.isUrlFromGlobalSettings : false
                            text: "Managed by /etc/nixbit.conf"
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }

                        TextField {
                            id: hostnameField
                            Kirigami.FormData.label: "Hostname:"
                            Layout.fillWidth: true
                            text: settingsManager ? settingsManager.hostname : ""
                            placeholderText: "Flake configuration to build, e.g. my-laptop"
                            enabled: !root.gitBusy && !root.processRunning
                            onEditingFinished: {
                                if (settingsManager)
                                    settingsManager.hostname = text;
                            }
                        }

                        ComboBox {
                            id: rebuildModeComboBox
                            Kirigami.FormData.label: "Rebuild mode:"
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 8
                            model: ["build", "switch", "boot"]
                            currentIndex: (typeof isDebugMode !== 'undefined' && isDebugMode) ? 0 : 1
                            enabled: !root.gitBusy && !root.processRunning
                            onCurrentTextChanged: {
                                // Restore build host selector when mode text changes
                                console.log("Mode changed to:", currentText);
                                buildHostComboBox.restoreSelectionForMode(currentText);
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
                            text: rebuildModeComboBox.currentText === "build" ? "Builds the configuration without applying it. No administrator rights required." : rebuildModeComboBox.currentText === "switch" ? "Builds and activates the configuration, and makes it the boot default. Requires administrator rights." : "Builds the configuration and makes it the boot default without activating it now. Requires administrator rights."
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                            wrapMode: Text.WordWrap
                        }

                        ComboBox {
                            id: buildHostComboBox
                            Kirigami.FormData.label: "Build host:"
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 8
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
                            enabled: !root.gitBusy && !root.processRunning
                            ToolTip.visible: hovered
                            ToolTip.text: "Host to build on. Each rebuild mode remembers its own selection. Build hosts can be added in the settings."

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
                                } else if (currentMode === "boot") {
                                    settingsManager.selectedBootHost = selectedHost;
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

                                var selectedHost = mode === "build" ? settingsManager.selectedBuildHost : mode === "boot" ? settingsManager.selectedBootHost : settingsManager.selectedSwitchHost;
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

                        RowLayout {
                            Kirigami.FormData.label: "Status:"
                            spacing: Kirigami.Units.smallSpacing

                            GitStatusIndicator {}

                            Label {
                                text: root.gitStatus
                                color: root.gitStatusIsError ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.textColor
                            }
                        }

                        RowLayout {
                            Kirigami.FormData.label: "Updates:"
                            spacing: Kirigami.Units.largeSpacing

                            Label {
                                text: root.commitsBehind > 0 ? (root.commitsBehind === 1 ? "1 new commit" : root.commitsBehind + " new commits") : root.commitsBehind === 0 ? "Up to date" : "Unknown"
                                font.bold: root.commitsBehind > 0
                                color: root.commitsBehind > 0 ? Kirigami.Theme.neutralTextColor : root.commitsBehind === 0 ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.disabledTextColor
                            }

                            Button {
                                text: "View Commits…"
                                icon.name: "view-list-details"
                                visible: root.commitsBehind > 0
                                onClicked: commitsDialog.open()
                            }
                        }
                    }
                }
            }

            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: rebuildModeComboBox.currentText === "build" ? "Build System" : rebuildModeComboBox.currentText === "switch" ? "Update System" : "Set Boot Default"
                    icon.name: "system-software-update"
                    highlighted: true
                    enabled: !root.processRunning && !root.gitBusy
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: "Pull the latest changes, then run " + rebuildModeComboBox.currentText
                    onClicked: {
                        // First pull the repository, then update system
                        if (gitManager) {
                            root.shouldRebuildAfterPull = true;
                            gitManager.pullRepository();
                        }
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: "Build & Switch"
                    icon.name: "run-build"
                    enabled: !root.processRunning && !root.gitBusy
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: "Pull the latest changes, build on the build host, then switch if the build succeeds"
                    onClicked: {
                        if (!gitManager)
                            return;

                        // Set the flags to indicate we're doing a chained operation and should rebuild
                        root.isChainedBuildSwitch = true;
                        root.shouldRebuildAfterPull = true;

                        // First pull the repository, then build and switch
                        gitManager.pullRepository();
                    }
                }

                Button {
                    Layout.fillWidth: true
                    Layout.preferredWidth: 1
                    text: "Check for Updates"
                    icon.name: "view-refresh"
                    enabled: !root.gitBusy
                    onClicked: {
                        if (gitManager)
                            gitManager.checkForUpdates();
                    }
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

            ProgressBar {
                Layout.fillWidth: true
                visible: root.gitBusy && root.gitStatus.indexOf("Cloning") !== -1
                from: 0
                to: 100
                value: gitManager ? gitManager.progress : 0
            }

            // NixOS Generations Section
            GroupBox {
                title: "NixOS Generations"
                Layout.fillWidth: true
                visible: !root.processRunning

                RowLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    Label {
                        text: "Current generation:"
                    }

                    Label {
                        text: generationManager && generationManager.currentGenerationNumber > 0 ? "#" + generationManager.currentGenerationNumber : "Unknown"
                        font.bold: true
                        color: Kirigami.Theme.positiveTextColor
                    }

                    Label {
                        text: "·"
                        color: Kirigami.Theme.disabledTextColor
                        visible: dateLabel.visible
                    }

                    Label {
                        id: dateLabel
                        Layout.fillWidth: true
                        text: generationManager && generationManager.currentGenerationDate ? generationManager.currentGenerationDate : ""
                        visible: text !== ""
                        color: Kirigami.Theme.disabledTextColor
                        elide: Text.ElideRight
                    }

                    Item {
                        Layout.fillWidth: true
                        visible: !dateLabel.visible
                    }

                    ToolButton {
                        icon.name: "view-refresh"
                        display: AbstractButton.IconOnly
                        text: "Reload generations"
                        ToolTip.visible: hovered
                        ToolTip.text: text
                        enabled: generationManager ? !generationManager.isLoading : false
                        onClicked: {
                            if (generationManager)
                                generationManager.loadGenerations();
                        }
                    }

                    Button {
                        icon.name: "view-history"
                        text: "All Generations…"
                        onClicked: generationsDialog.open()
                    }
                }

                Component.onCompleted: {
                    if (generationManager)
                        generationManager.loadGenerations();
                }
            }

            GenerationsDialog {
                id: generationsDialog
            }

            CommitsDialog {
                id: commitsDialog
            }

            LogsDialog {
                id: logsDialog
            }

            // System Resources Section
            GroupBox {
                title: "System Resources"
                Layout.fillWidth: true
                visible: systemMonitor ? systemMonitor.active : false

                RowLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.largeSpacing

                    ResourceMetric {
                        caption: "CPU"
                        percent: systemMonitor ? systemMonitor.cpuUsage : 0
                        value: systemMonitor ? systemMonitor.cpuUsage.toFixed(1) + " %" : "0.0 %"
                    }

                    ResourceMetric {
                        caption: "Memory"
                        percent: systemMonitor ? systemMonitor.memoryUsage : 0
                        value: systemMonitor ? systemMonitor.usedMemory + " / " + systemMonitor.totalMemory : ""
                    }

                    ResourceMetric {
                        caption: "Network"
                        value: systemMonitor ? systemMonitor.networkStats : "↓ 0 B/s ↑ 0 B/s"
                    }

                    ResourceMetric {
                        caption: "Disk I/O"
                        value: systemMonitor ? systemMonitor.diskStats : "↓ 0 B/s ↑ 0 B/s"
                    }

                    ResourceMetric {
                        caption: "Load"
                        value: systemMonitor ? systemMonitor.systemLoad.toFixed(2) : "0.00"
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
                    spacing: Kirigami.Units.smallSpacing

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
                            var mode = root.rebuildModeTitle(root.currentRebuildMode);
                            if (processManager.lastExitCode === 0) {
                                return mode + " completed successfully.";
                            } else {
                                return mode + " failed with exit code " + processManager.lastExitCode + ".";
                            }
                        }
                        showCloseButton: true
                    }

                    // Shown while e.g. nh waits for the sudo password in the terminal
                    Kirigami.InlineMessage {
                        id: passwordPromptMessage
                        Layout.fillWidth: true
                        visible: root.kwiTerminalShown && processManager !== null && processManager.waitingForPassword
                        type: Kirigami.MessageType.Warning
                        icon.name: "dialog-password"
                        text: "Password required: type your password in the terminal below and press Enter to continue."
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumHeight: Kirigami.Units.gridUnit * 6
                        color: "#0d1117"
                        border.color: "#30363d"
                        border.width: 1
                        radius: Kirigami.Units.cornerRadius

                        // Optional embedded Kwi terminal, falls back to the text view
                        // below if the org.kde.kwi.terminal module isn't installed
                        Loader {
                            id: kwiTerminalLoader
                            anchors.fill: parent
                            anchors.margins: 1
                            source: "KwiTerminalPane.qml"
                            visible: root.kwiTerminalShown
                            onLoaded: item.scrollbackLines = Qt.binding(function () {
                                return processManager ? processManager.maxOutputLines : -1;
                            })
                        }

                        ScrollView {
                            id: terminalScrollView
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing
                            clip: true
                            visible: !root.kwiTerminalShown
                            ScrollBar.vertical.policy: ScrollBar.AlwaysOn

                            Flickable {
                                id: terminalFlickable
                                contentWidth: terminalText.width
                                contentHeight: terminalText.height
                                boundsBehavior: Flickable.StopAtBounds

                                property bool autoScroll: true

                                function scrollToBottom() {
                                    if (contentHeight > height) {
                                        contentY = contentHeight - height;
                                    }
                                }

                                function isAtBottom() {
                                    // Consider "at bottom" if within 50 pixels of the bottom
                                    return contentHeight <= height || contentY >= (contentHeight - height - 50);
                                }

                                onContentYChanged: {
                                    // Update autoScroll based on user's scroll position
                                    autoScroll = isAtBottom();
                                }

                                TextEdit {
                                    id: terminalText
                                    width: terminalScrollView.availableWidth
                                    textFormat: TextEdit.RichText
                                    wrapMode: TextEdit.Wrap
                                    font.family: Kirigami.Theme.fixedWidthFont.family
                                    font.pointSize: Kirigami.Theme.fixedWidthFont.pointSize
                                    color: "#c9d1d9"
                                    selectionColor: "#264f78"
                                    selectedTextColor: "#ffffff"
                                    readOnly: true
                                    selectByMouse: true
                                    selectByKeyboard: true
                                    focus: true

                                    text: {
                                        if (!processManager || !processManager.output) {
                                            return "";
                                        }
                                        // Use C++ highlighter for better performance
                                        return outputHighlighter ? outputHighlighter.highlight(processManager.output) : processManager.output;
                                    }

                                    onTextChanged: {
                                        // Only auto-scroll if user hasn't scrolled up and search bar is not visible
                                        if (terminalFlickable.autoScroll && !searchBar.visible) {
                                            Qt.callLater(terminalFlickable.scrollToBottom);
                                        }
                                    }

                                    Component.onDestruction: {
                                        // Clear text to help release memory
                                        text = "";
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
                                            action: Action {
                                                text: "Copy"
                                                icon.name: "edit-copy"
                                                shortcut: StandardKey.Copy
                                                enabled: terminalText.selectedText.length > 0
                                                onTriggered: terminalText.copy()
                                            }
                                        }

                                        MenuItem {
                                            action: Action {
                                                text: "Select All"
                                                icon.name: "edit-select-all"
                                                shortcut: StandardKey.SelectAll
                                                onTriggered: terminalText.selectAll()
                                            }
                                        }

                                        MenuSeparator {}

                                        MenuItem {
                                            text: "Deselect"
                                            enabled: terminalText.selectedText.length > 0
                                            onTriggered: terminalText.deselect()
                                        }

                                        MenuItem {
                                            text: "Find…"
                                            icon.name: "edit-find"
                                            onTriggered: searchBar.show()
                                        }
                                    }
                                }
                            }
                        }

                        // Placeholder while there is no output yet
                        Label {
                            anchors.centerIn: parent
                            width: parent.width - Kirigami.Units.gridUnit * 4
                            visible: !root.kwiTerminalShown && !root.processRunning && terminalText.length === 0
                            horizontalAlignment: Text.AlignHCenter
                            wrapMode: Text.WordWrap
                            text: "Command output will appear here."
                            color: "#6e7681"
                        }

                        // Scroll to bottom button (appears when scrolled up)
                        Button {
                            id: scrollToBottomButton
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                            anchors.rightMargin: Kirigami.Units.gridUnit
                            anchors.bottomMargin: Kirigami.Units.largeSpacing
                            visible: !terminalFlickable.autoScroll && !root.kwiTerminalShown
                            icon.name: "go-down-skip"
                            text: "Scroll to Bottom"
                            ToolTip.visible: hovered
                            ToolTip.text: "Jump to the end of the output and follow new lines"
                            onClicked: {
                                terminalFlickable.scrollToBottom();
                                terminalFlickable.autoScroll = true;
                            }
                            z: 10
                        }
                    }

                    // Search Bar (at bottom of log area)
                    Rectangle {
                        id: searchBar
                        Layout.fillWidth: true
                        implicitHeight: searchRow.implicitHeight + Kirigami.Units.smallSpacing * 2
                        visible: false
                        color: Kirigami.Theme.backgroundColor
                        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, Kirigami.Theme.frameContrast)
                        border.width: 1
                        radius: Kirigami.Units.cornerRadius

                        property var searchMatches: []
                        property int currentMatchIndex: -1
                        property string plainTextCache: ""

                        function getPlainText() {
                            // Get plain text from the cached output (before HTML highlighting)
                            if (processManager && processManager.output) {
                                return processManager.output;
                            }
                            return "";
                        }

                        function findMatches(searchTerm) {
                            var matches = [];
                            if (!searchTerm || searchTerm.length === 0) {
                                return matches;
                            }
                            var plainText = getPlainText();
                            if (!plainText || plainText.length === 0) {
                                return matches;
                            }
                            var lowerText = plainText.toLowerCase();
                            var lowerSearch = searchTerm.toLowerCase();
                            var pos = 0;
                            while ((pos = lowerText.indexOf(lowerSearch, pos)) !== -1) {
                                matches.push(pos);
                                pos += searchTerm.length;
                            }
                            return matches;
                        }

                        function search() {
                            var searchTerm = searchInput.text;

                            if (root.kwiTerminalShown) {
                                var view = kwiTerminalLoader.item.view;
                                if (!searchTerm || searchTerm.length === 0) {
                                    view.clearSearch();
                                } else {
                                    view.find(searchTerm, true);
                                }
                                return;
                            }

                            if (!searchTerm || searchTerm.length === 0) {
                                searchMatches = [];
                                currentMatchIndex = -1;
                                searchResultLabel.text = "";
                                terminalText.deselect();
                                return;
                            }

                            // Disable auto-scroll as soon as we start searching
                            terminalFlickable.autoScroll = false;

                            searchMatches = findMatches(searchTerm);

                            if (searchMatches.length > 0) {
                                currentMatchIndex = 0;
                                highlightMatch();
                                searchResultLabel.text = (currentMatchIndex + 1) + "/" + searchMatches.length;
                            } else {
                                currentMatchIndex = -1;
                                searchResultLabel.text = "0/0";
                                terminalText.deselect();
                            }
                        }

                        function highlightMatch() {
                            if (currentMatchIndex < 0 || currentMatchIndex >= searchMatches.length) {
                                return;
                            }
                            var plainText = getPlainText();
                            var pos = searchMatches[currentMatchIndex];
                            var len = searchInput.text.length;

                            // We need to find the position in the rich text that corresponds to
                            // the plain text position. Since the highlighter wraps text in HTML,
                            // we can't directly use plain text positions.
                            // Instead, let's search in the actual TextEdit text (which is rendered plain)
                            var displayText = terminalText.getText(0, terminalText.length);
                            var searchTerm = searchInput.text.toLowerCase();

                            // Find the nth occurrence in the display text
                            var displayPos = -1;
                            var searchPos = 0;
                            for (var i = 0; i <= currentMatchIndex; i++) {
                                displayPos = displayText.toLowerCase().indexOf(searchTerm, searchPos);
                                if (displayPos === -1)
                                    break;
                                searchPos = displayPos + 1;
                            }

                            if (displayPos !== -1) {
                                terminalText.select(displayPos, displayPos + len);

                                // Disable auto-scroll when manually navigating search results
                                terminalFlickable.autoScroll = false;

                                // Scroll to make selection visible - always scroll to center the match
                                var lineHeight = terminalText.font.pixelSize * 1.5;
                                var estimatedY = (displayPos / 80) * lineHeight;

                                // Always scroll to center the match in the viewport
                                var targetY = Math.max(0, Math.min(estimatedY - terminalFlickable.height / 2, terminalFlickable.contentHeight - terminalFlickable.height));

                                terminalFlickable.contentY = targetY;
                            }
                        }

                        function findNext() {
                            if (root.kwiTerminalShown) {
                                if (searchInput.text.length > 0)
                                    kwiTerminalLoader.item.view.find(searchInput.text, true);
                                return;
                            }
                            if (searchMatches.length === 0) {
                                search();
                                return;
                            }
                            currentMatchIndex = (currentMatchIndex + 1) % searchMatches.length;
                            highlightMatch();
                            searchResultLabel.text = (currentMatchIndex + 1) + "/" + searchMatches.length;
                        }

                        function findPrevious() {
                            if (root.kwiTerminalShown) {
                                if (searchInput.text.length > 0)
                                    kwiTerminalLoader.item.view.find(searchInput.text, false);
                                return;
                            }
                            if (searchMatches.length === 0) {
                                search();
                                return;
                            }
                            currentMatchIndex = (currentMatchIndex - 1 + searchMatches.length) % searchMatches.length;
                            highlightMatch();
                            searchResultLabel.text = (currentMatchIndex + 1) + "/" + searchMatches.length;
                        }

                        function show() {
                            visible = true;
                            // Disable auto-scroll as soon as search bar is shown
                            terminalFlickable.autoScroll = false;
                            searchInput.forceActiveFocus();
                            searchInput.selectAll();
                        }

                        function hide() {
                            visible = false;
                            searchMatches = [];
                            currentMatchIndex = -1;
                            searchResultLabel.text = "";
                            plainTextCache = "";  // Clear cached plain text
                            terminalText.deselect();
                            if (root.kwiTerminalShown) {
                                kwiTerminalLoader.item.view.clearSearch();
                                kwiTerminalLoader.item.forceActiveFocus();
                            } else {
                                terminalText.forceActiveFocus();
                            }
                        }

                        RowLayout {
                            id: searchRow
                            anchors.fill: parent
                            anchors.margins: Kirigami.Units.smallSpacing
                            spacing: Kirigami.Units.smallSpacing

                            TextField {
                                id: searchInput
                                Layout.fillWidth: true
                                placeholderText: "Find in output…"
                                leftPadding: searchIcon.width + Kirigami.Units.smallSpacing * 3
                                onTextChanged: searchBar.search()

                                Kirigami.Icon {
                                    id: searchIcon
                                    anchors.left: parent.left
                                    anchors.leftMargin: Kirigami.Units.smallSpacing * 2
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: Kirigami.Units.iconSizes.small
                                    height: width
                                    source: "edit-find"
                                    color: Kirigami.Theme.disabledTextColor
                                }

                                Keys.onPressed: function (event) {
                                    if (event.key === Qt.Key_Escape) {
                                        searchBar.hide();
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Return || event.key === Qt.Key_Enter) {
                                        if (event.modifiers & Qt.ShiftModifier) {
                                            searchBar.findPrevious();
                                        } else {
                                            searchBar.findNext();
                                        }
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_F3) {
                                        if (event.modifiers & Qt.ShiftModifier) {
                                            searchBar.findPrevious();
                                        } else {
                                            searchBar.findNext();
                                        }
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Down) {
                                        searchBar.findNext();
                                        event.accepted = true;
                                    } else if (event.key === Qt.Key_Up) {
                                        searchBar.findPrevious();
                                        event.accepted = true;
                                    }
                                }
                            }

                            Label {
                                id: searchResultLabel
                                text: ""
                                visible: !root.kwiTerminalShown
                                color: text === "0/0" ? Kirigami.Theme.negativeTextColor : Kirigami.Theme.disabledTextColor
                                horizontalAlignment: Text.AlignHCenter
                                Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                            }

                            Label {
                                text: root.kwiTerminalShown ? kwiTerminalLoader.item.view.searchStatus : ""
                                visible: root.kwiTerminalShown
                                color: Kirigami.Theme.disabledTextColor
                            }

                            ToolButton {
                                icon.name: "go-up"
                                display: AbstractButton.IconOnly
                                text: "Find Previous (Shift+F3)"
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                enabled: root.kwiTerminalShown ? searchInput.text.length > 0 : searchBar.searchMatches.length > 0
                                onClicked: searchBar.findPrevious()
                            }

                            ToolButton {
                                icon.name: "go-down"
                                display: AbstractButton.IconOnly
                                text: "Find Next (F3)"
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                enabled: root.kwiTerminalShown ? searchInput.text.length > 0 : searchBar.searchMatches.length > 0
                                onClicked: searchBar.findNext()
                            }

                            ToolButton {
                                icon.name: "dialog-close"
                                display: AbstractButton.IconOnly
                                text: "Close (Escape)"
                                ToolTip.visible: hovered
                                ToolTip.text: text
                                onClicked: searchBar.hide()
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Kirigami.Units.smallSpacing

                        Item {
                            implicitWidth: Kirigami.Units.iconSizes.small
                            implicitHeight: Kirigami.Units.iconSizes.small

                            BusyIndicator {
                                anchors.centerIn: parent
                                width: Kirigami.Units.iconSizes.smallMedium
                                height: width
                                padding: 0
                                running: root.processRunning && !(processManager && processManager.isPaused)
                                visible: running
                            }

                            Kirigami.Icon {
                                anchors.fill: parent
                                visible: !root.processRunning || (processManager && processManager.isPaused)
                                source: processManager && processManager.isPaused ? "media-playback-pause" : "emblem-success"
                            }
                        }

                        Label {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: !root.processRunning ? "Ready" : processManager.isPaused ? "Paused" : processManager.waitingForPassword ? "Waiting for password…" : "Running…"
                            color: processManager && processManager.isPaused ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.textColor
                        }

                        // Debug Mode Controls
                        SpinBox {
                            id: testLineCountSpinBox
                            visible: settingsManager && settingsManager.debugMode
                            from: 100
                            to: 50000
                            stepSize: 1000
                            value: 5000
                            editable: true
                            ToolTip.visible: hovered
                            ToolTip.text: "Number of test lines to generate"
                        }

                        Button {
                            icon.name: "tools-report-bug"
                            text: "Generate Test Output"
                            visible: settingsManager && settingsManager.debugMode
                            enabled: !root.processRunning
                            ToolTip.visible: hovered
                            ToolTip.text: "Generate test log output to check memory usage (Debug Mode)"
                            onClicked: {
                                if (processManager) {
                                    processManager.generateTestOutput(testLineCountSpinBox.value);
                                }
                            }
                        }

                        Button {
                            text: processManager && processManager.isPaused ? "Resume" : "Pause"
                            icon.name: processManager && processManager.isPaused ? "media-playback-start" : "media-playback-pause"
                            // Privileged rebuilds run as root, but nh only elevates for the activation
                            visible: root.processRunning && (root.nhUsed || root.currentRebuildMode === "build")
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
                            text: "Stop"
                            icon.name: "process-stop"
                            visible: root.processRunning
                            ToolTip.visible: hovered
                            ToolTip.delay: Kirigami.Units.toolTipDelay
                            ToolTip.text: "Kill the running process"
                            onClicked: {
                                if (processManager)
                                    processManager.killProcess();
                            }
                        }

                        Button {
                            text: "Clear"
                            icon.name: "edit-clear-history"
                            enabled: !root.processRunning
                            onClicked: {
                                if (processManager) {
                                    // Clear output in process manager first
                                    processManager.clearOutput();

                                    // Clear search state to prevent memory leaks
                                    searchBar.searchMatches = [];
                                    searchBar.currentMatchIndex = -1;
                                    searchBar.plainTextCache = "";
                                    searchResultLabel.text = "";

                                    // Force garbage collection hint
                                    gc();
                                }
                            }
                        }

                        Button {
                            icon.name: "documentation"
                            text: "Build Logs…"
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
            // Pull completed successfully
            if (!settingsManager || !gitManager || !processManager)
                return;

            // Check if we should run a rebuild after pull
            if (!root.shouldRebuildAfterPull) {
                // Just pulled to update git, no rebuild needed
                messageBox.text = "Repository updated successfully. Changes will be applied in the next build.";
                messageBox.type = Kirigami.MessageType.Positive;
                messageBox.visible = true;
                return;
            }

            // Reset the flag
            root.shouldRebuildAfterPull = false;

            // Now run system update
            var hostname = settingsManager.hostname;
            var repoPath = gitManager.localPath;
            var buildHost = "";

            // Check if we're doing a chained build-switch operation
            if (root.isChainedBuildSwitch) {
                // Get the build host for the build phase
                var selectedBuildHost = settingsManager.selectedBuildHost;

                if (selectedBuildHost && selectedBuildHost !== "" && selectedBuildHost !== "(local)") {
                    buildHost = settingsManager.getBuildHostAddress(selectedBuildHost);
                }

                messageBox.text = "Starting build phase...";
                messageBox.type = Kirigami.MessageType.Information;
                messageBox.visible = true;

                // Use C++ method to run build
                root.currentRebuildMode = "build";
                processManager.runNixosRebuildBuild(repoPath, hostname, buildHost);
            } else {
                // Normal single operation
                var rebuildMode = rebuildModeComboBox.currentText;

                // Get the selected build host based on the current mode
                // Boot mode uses its own host selection (like switch mode)
                var selectedHost = rebuildMode === "build" ? settingsManager.selectedBuildHost : rebuildMode === "boot" ? settingsManager.selectedBootHost : settingsManager.selectedSwitchHost;

                // If a host is selected, get its address
                if (selectedHost && selectedHost !== "" && selectedHost !== "(local)") {
                    buildHost = settingsManager.getBuildHostAddress(selectedHost);
                }

                // Use the C++ methods to run the commands
                if (rebuildMode === "switch") {
                    root.currentRebuildMode = "switch";
                    processManager.runNixosRebuildSwitch(repoPath, hostname, buildHost);
                } else if (rebuildMode === "boot") {
                    root.currentRebuildMode = "boot";
                    processManager.runNixosRebuildBoot(repoPath, hostname, buildHost);
                } else {
                    root.currentRebuildMode = "build";
                    processManager.runNixosRebuildBuild(repoPath, hostname, buildHost);
                }
            }
        }
    }

    Connections {
        target: processManager

        function onIsRunningChanged() {
            if (systemMonitor) {
                systemMonitor.active = processManager.isRunning;
            }
            // Refresh generations when process completes (in case a new generation was created)
            // Only refresh after switch or boot mode since build mode doesn't create a new generation
            // Use currentRebuildMode to check actual operation type (handles Build & Switch correctly)
            if (!processManager.isRunning && generationManager && (root.currentRebuildMode === "switch" || root.currentRebuildMode === "boot")) {
                generationManager.loadGenerations();
            }
        }

        function onTerminalCommandRequested(runId, program, args, workingDirectory) {
            if (!root.kwiTerminalAvailable) {
                processManager.terminalCommandFinished(runId, 127, "", "The embedded terminal is not available");
                return;
            }
            searchBar.hide();
            kwiTerminalLoader.item.run(runId, program, args, workingDirectory);
        }

        function onTerminalCloseRequested(runId) {
            if (root.kwiTerminalAvailable) {
                kwiTerminalLoader.item.closeSession(runId);
            }
        }

        function onPauseRefused(reason) {
            messageBox.text = reason;
            messageBox.type = Kirigami.MessageType.Warning;
            messageBox.visible = true;
        }

        function onWaitingForPasswordChanged() {
            if (processManager.waitingForPassword && root.kwiTerminalAvailable) {
                kwiTerminalLoader.item.focusTerminal();
            }
        }

        function onTerminalResetRequested() {
            if (root.kwiTerminalAvailable) {
                kwiTerminalLoader.item.reset();
            }
        }

        function onOutputChanged() {
            // Always clear search results when output changes to prevent memory leaks
            if (searchBar.searchMatches.length > 0) {
                searchBar.searchMatches = [];
                searchBar.currentMatchIndex = -1;
                searchResultLabel.text = "";
                searchBar.plainTextCache = "";
            }
        }

        function onCommandFinished(exitCode, outputFilePath) {
            // Save build log when command finishes
            if (logManager && settingsManager && outputFilePath.length > 0) {
                var logDir = settingsManager.getLogDirectory();
                var maxLogs = settingsManager.maxStoredLogs;
                var buildType = root.currentRebuildMode;
                logManager.saveLog(outputFilePath, exitCode, logDir, maxLogs, buildType);
            }

            // Trigger garbage collection after build completes
            gc();

            // Handle chained build-switch operation
            if (root.isChainedBuildSwitch) {
                if (exitCode === 0) {
                    // Build succeeded, now run switch
                    messageBox.text = "Build completed successfully! Starting switch phase...";
                    messageBox.type = Kirigami.MessageType.Positive;
                    messageBox.visible = true;

                    // Reset the flag before starting switch
                    root.isChainedBuildSwitch = false;

                    if (!settingsManager || !gitManager || !processManager)
                        return;

                    var hostname = settingsManager.hostname;
                    var repoPath = gitManager.localPath;

                    // Get the switch host for the switch phase
                    var selectedSwitchHost = settingsManager.selectedSwitchHost;
                    var switchHost = "";

                    if (selectedSwitchHost && selectedSwitchHost !== "" && selectedSwitchHost !== "(local)") {
                        switchHost = settingsManager.getBuildHostAddress(selectedSwitchHost);
                    }

                    // Use C++ method to run switch
                    root.currentRebuildMode = "switch";
                    processManager.runNixosRebuildSwitch(repoPath, hostname, switchHost);
                } else {
                    // Build failed, reset the flag
                    root.isChainedBuildSwitch = false;
                    messageBox.text = "Build failed with exit code " + exitCode + ". Switch phase cancelled.";
                    messageBox.type = Kirigami.MessageType.Error;
                    messageBox.visible = true;
                }
            }
        }
    }
}
