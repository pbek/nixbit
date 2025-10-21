import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "Nixbit - NixOS Updater v" + appVersion
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
                    enabled: gitManager ? !gitManager.isBusy : false
                    onEditingFinished: {
                        if (gitManager)
                            gitManager.repositoryUrl = text;
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
                            var cmd = "if command -v konsole >/dev/null 2>&1; then konsole --workdir '" + path + "' & " + "elif command -v gnome-terminal >/dev/null 2>&1; then gnome-terminal --working-directory='" + path + "' & " + "elif command -v xfce4-terminal >/dev/null 2>&1; then xfce4-terminal --working-directory='" + path + "' & " + "elif command -v alacritty >/dev/null 2>&1; then alacritty --working-directory '" + path + "' & " + "elif command -v kitty >/dev/null 2>&1; then kitty --directory '" + path + "' & " + "elif command -v ghostty >/dev/null 2>&1; then ghostty --working-directory='" + path + "' & " + "elif command -v xterm >/dev/null 2>&1; then cd '" + path + "' && xterm & " + "else notify-send 'NixBit' 'No supported terminal emulator found'; fi";
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

                CheckBox {
                    Kirigami.FormData.label: "Start Hidden:"
                    checked: settingsManager ? settingsManager.startHidden : false
                    text: "Start application hidden in system tray"
                    onToggled: {
                        if (settingsManager)
                            settingsManager.startHidden = checked;
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

            // Terminal Output Section
            GroupBox {
                title: "Terminal Output"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 5

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#1e1e1e"
                        border.color: "#3e3e3e"
                        border.width: 1
                        radius: 4

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 5
                            clip: true

                            TextArea {
                                id: terminalOutput
                                readOnly: true
                                textFormat: TextEdit.PlainText
                                wrapMode: TextEdit.Wrap
                                font.family: "Monospace"
                                font.pixelSize: 12
                                color: "#00ff00"
                                text: processManager ? processManager.output : ""
                                background: Rectangle {
                                    color: "transparent"
                                }

                                // Auto-scroll to bottom when output changes
                                onTextChanged: {
                                    cursorPosition = text.length;
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true

                        Label {
                            text: processManager ? (processManager.isRunning ? "Process running..." : "Ready") : "Ready"
                            color: processManager ? (processManager.isRunning ? "#ffaa00" : "#00ff00") : "#00ff00"
                            font.family: "Monospace"
                            font.pixelSize: 10
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
            var tempScript = "/tmp/nixbit-rebuild-" + Date.now() + ".sh";

            // Build mode doesn't need sudo, switch mode does
            var cmd = "printf '#!/usr/bin/env bash\\n" + "set -e\\n" + "TEMP_REPO=/tmp/nixbit-repo-$$\\n" + "echo \\\"Copying repository to temporary location...\\\"\\n" + "cp -r " + repoPath + " $TEMP_REPO\\n" + "cd $TEMP_REPO\\n" + "nixos-rebuild " + rebuildMode + " --flake .#" + hostname + " -L\\n" + "echo \\\"Cleaning up temporary repository...\\\"\\n" + "rm -rf $TEMP_REPO\\n' > " + tempScript + " && chmod +x " + tempScript;

            // Only use pkexec for switch mode
            if (rebuildMode === "switch") {
                cmd += " && pkexec " + tempScript + " ; rm -f " + tempScript;
            } else {
                cmd += " && " + tempScript + " ; rm -f " + tempScript;
            }

            processManager.runCommand("bash", ["-c", cmd]);
        }
    }
}
