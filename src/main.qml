import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.nixbit 1.0

Kirigami.ApplicationWindow {
    id: root
    title: "NixOS Updater"
    width: 1000
    height: 700

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
        title: "Repository Manager"

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

                RowLayout {
                    Kirigami.FormData.label: "Local Path:"
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    TextEdit {
                        id: localPathLabel
                        text: gitManager ? gitManager.localPath : ""
                        readOnly: true
                        selectByMouse: true
                        wrapMode: Text.WrapAnywhere
                        color: Kirigami.Theme.textColor
                        Layout.fillWidth: true
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
                            if (!gitManager)
                                return;
                            // Build command with proper syntax for each terminal emulator
                            var path = gitManager.localPath;
                            var cmd = "if command -v konsole >/dev/null 2>&1; then konsole --workdir '" + path + "' & " + "elif command -v gnome-terminal >/dev/null 2>&1; then gnome-terminal --working-directory='" + path + "' & " + "elif command -v xfce4-terminal >/dev/null 2>&1; then xfce4-terminal --working-directory='" + path + "' & " + "elif command -v alacritty >/dev/null 2>&1; then alacritty --working-directory '" + path + "' & " + "elif command -v kitty >/dev/null 2>&1; then kitty --directory '" + path + "' & " + "elif command -v ghostty >/dev/null 2>&1; then ghostty --working-directory='" + path + "' & " + "elif command -v xterm >/dev/null 2>&1; then cd '" + path + "' && xterm & " + "else notify-send 'Nixbit' 'No supported terminal emulator found'; fi";
                            if (processManager)
                                processManager.startDetached("bash", ["-c", cmd]);
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
                    currentIndex: 1
                    enabled: (gitManager ? !gitManager.isBusy : false) && (processManager ? !processManager.isRunning : false)
                    ToolTip.visible: hovered
                    ToolTip.text: currentIndex === 0 ? "Build the system without activating (no sudo required)" : "Build and activate the new system (requires sudo)"
                }

                Label {
                    Kirigami.FormData.label: "Status:"
                    text: gitManager ? gitManager.status : ""
                    font.bold: true
                    color: gitManager ? (gitManager.isBusy ? Kirigami.Theme.activeTextColor : Kirigami.Theme.positiveTextColor) : Kirigami.Theme.positiveTextColor
                }

                Label {
                    Kirigami.FormData.label: "Commits Behind:"
                    text: gitManager ? (gitManager.commitsBehind >= 0 ? gitManager.commitsBehind.toString() : "N/A") : "N/A"
                    font.bold: gitManager ? gitManager.commitsBehind > 0 : false
                    color: gitManager ? (gitManager.commitsBehind > 0 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor) : Kirigami.Theme.positiveTextColor
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
                            font.pixelSize: 10
                        }
                        ProgressBar {
                            Layout.preferredWidth: 80
                            from: 0
                            to: 100
                            value: systemMonitor ? systemMonitor.cpuUsage : 0
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.cpuUsage.toFixed(1) + "%" : "0.0%"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // RAM
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "RAM"
                            font.bold: true
                            font.pixelSize: 10
                        }
                        ProgressBar {
                            Layout.preferredWidth: 80
                            from: 0
                            to: 100
                            value: systemMonitor ? systemMonitor.memoryUsage : 0
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.usedMemory + " / " + systemMonitor.totalMemory : ""
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Network
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "Network"
                            font.bold: true
                            font.pixelSize: 10
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.networkStats : "↓ 0 B/s ↑ 0 B/s"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }

                    // Load
                    ColumnLayout {
                        spacing: 2
                        Label {
                            text: "Load"
                            font.bold: true
                            font.pixelSize: 10
                        }
                        Label {
                            text: systemMonitor ? systemMonitor.systemLoad.toFixed(2) : "0.00"
                            font.pixelSize: 10
                            horizontalAlignment: Text.AlignHCenter
                        }
                    }
                }
            }

            // Embedded Terminal Section
            GroupBox {
                title: "Embedded Terminal"
                Layout.fillWidth: true
                Layout.fillHeight: true

                KonsolePartWidget {
                    id: embeddedTerminal
                    anchors.fill: parent
                    workingDirectory: gitManager ? gitManager.localPath : ""
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
            // Pull completed successfully, now run system update in embedded terminal
            if (!settingsManager || !gitManager)
                return;
            var hostname = settingsManager.hostname;
            var repoPath = gitManager.localPath;
            var rebuildMode = rebuildModeComboBox.currentText;

            // Send commands to embedded terminal
            embeddedTerminal.sendCommand("cd '" + repoPath + "'");
            if (rebuildMode === "switch") {
                embeddedTerminal.sendCommand("pkexec nixos-rebuild switch --flake .#" + hostname + " -L");
            } else {
                embeddedTerminal.sendCommand("nixos-rebuild build --flake .#" + hostname + " -L");
            }
        }
    }

    Connections {
        target: processManager

        function onIsRunningChanged() {
            if (systemMonitor) {
                systemMonitor.active = processManager.isRunning;
            }
        }
    }
}
