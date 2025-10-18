import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "NixBit - NixOS Updater"
    width: 1000
    height: 700

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "&Refresh Terminal"
                onTriggered: processManager.runCommand("gh", [])
            }
            MenuSeparator {}
            Action {
                text: "&Quit"
                onTriggered: Qt.quit()
            }
        }

        Menu {
            title: "&Tools"
            Action {
                text: "&Check for Updates"
                enabled: !gitManager.isBusy
                onTriggered: gitManager.checkForUpdates()
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
                    text: gitManager.repositoryUrl
                    placeholderText: "https://github.com/user/repo.git"
                    enabled: !gitManager.isBusy
                    onEditingFinished: {
                        gitManager.repositoryUrl = text;
                    }
                }

                TextEdit {
                    id: localPathLabel
                    Kirigami.FormData.label: "Local Path:"
                    text: gitManager.localPath
                    readOnly: true
                    selectByMouse: true
                    wrapMode: Text.WrapAnywhere
                    color: Kirigami.Theme.textColor
                    Layout.fillWidth: true
                }

                Label {
                    Kirigami.FormData.label: "Status:"
                    text: gitManager.status
                    font.bold: true
                    color: gitManager.isBusy ? Kirigami.Theme.activeTextColor : Kirigami.Theme.positiveTextColor
                }

                Label {
                    Kirigami.FormData.label: "Commits Behind:"
                    text: gitManager.commitsBehind >= 0 ? gitManager.commitsBehind.toString() : "N/A"
                    font.bold: gitManager.commitsBehind > 0
                    color: gitManager.commitsBehind > 0 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor
                }

                RowLayout {
                    Kirigami.FormData.label: "Auto-fetch Interval:"

                    SpinBox {
                        id: fetchIntervalSpinBox
                        from: 1
                        to: 60
                        value: gitManager.fetchIntervalMinutes
                        editable: true
                        enabled: !gitManager.isBusy
                        onValueModified: {
                            gitManager.fetchIntervalMinutes = value;
                        }
                    }

                    Label {
                        text: "minutes"
                    }
                }

                CheckBox {
                    Kirigami.FormData.label: "Start Hidden:"
                    checked: settingsManager.startHidden
                    text: "Start application hidden in system tray"
                    onToggled: {
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
                    enabled: !processManager.isRunning && !gitManager.isBusy
                    onClicked: {
                        // First pull the repository, then update system
                        gitManager.pullRepository();
                    }
                    Layout.fillWidth: true
                }

                Button {
                    text: "Check for Updates"
                    icon.name: "view-refresh"
                    enabled: !gitManager.isBusy
                    onClicked: {
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
                running: gitManager.isBusy
                visible: gitManager.isBusy
            }

            ProgressBar {
                Layout.fillWidth: true
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                visible: gitManager.isBusy && gitManager.status.indexOf("Cloning") !== -1
                from: 0
                to: 100
                value: gitManager.progress

                Component.onCompleted: {
                    console.log("ProgressBar created");
                }

                onVisibleChanged: {
                    console.log("ProgressBar visibility changed to:", visible);
                    console.log("  - isBusy:", gitManager.isBusy);
                    console.log("  - status:", gitManager.status);
                    console.log("  - progress:", gitManager.progress);
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
                                text: processManager.output
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
                            text: processManager.isRunning ? "Process running..." : "Ready"
                            color: processManager.isRunning ? "#ffaa00" : "#00ff00"
                            font.family: "Monospace"
                            font.pixelSize: 10
                        }

                        Item {
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Clear"
                            enabled: !processManager.isRunning
                            onClicked: {
                                processManager.runCommand("clear", []);
                            }
                        }

                        Button {
                            text: "Kill Process"
                            enabled: processManager.isRunning
                            onClicked: {
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
            var hostname = processManager.getHostname();
            var repoPath = gitManager.localPath;
            var tempScript = "/tmp/nixbit-rebuild-" + Date.now() + ".sh";
            var cmd = "printf '#!/usr/bin/env bash\\n" + "set -e\\n" + "TEMP_REPO=/tmp/nixbit-repo-$$\\n" + "echo \\\"Copying repository to temporary location...\\\"\\n" + "cp -r " + repoPath + " $TEMP_REPO\\n" + "cd $TEMP_REPO\\n" + "nixos-rebuild build --flake .#" + hostname + " -L\\n" + "echo \\\"Cleaning up temporary repository...\\\"\\n" + "rm -rf $TEMP_REPO\\n' > " + tempScript + " && chmod +x " + tempScript + " && pkexec " + tempScript + " ; rm -f " + tempScript;
            processManager.runCommand("bash", ["-c", cmd]);
        }
    }
}
