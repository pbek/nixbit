import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "NixBit - Git Repository Manager"
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
                text: "&Update System"
                onTriggered: processManager.runCommand("gh", [])
            }
            Action {
                text: "&Clone/Pull Repository"
                enabled: !gitManager.isBusy
                onTriggered: gitManager.cloneOrPullRepository()
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
                        gitManager.repositoryUrl = text
                    }
                }

                Label {
                    Kirigami.FormData.label: "Local Path:"
                    text: gitManager.localPath
                    wrapMode: Text.WrapAnywhere
                    Layout.fillWidth: true
                }

                Label {
                    Kirigami.FormData.label: "Status:"
                    text: gitManager.status
                    font.bold: true
                    color: gitManager.isBusy ? Kirigami.Theme.activeTextColor : Kirigami.Theme.positiveTextColor
                }
            }

            // Action Buttons
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Button {
                    text: "Update System"
                    icon.name: "system-software-update"
                    enabled: !processManager.isRunning
                    onClicked: {
                        var hostname = processManager.getHostname()
                        var repoPath = gitManager.localPath
                        // Write script to temp file and execute it with pkexec
                        var tempScript = "/tmp/nixbit-rebuild-" + Date.now() + ".sh"
                        // Copy repo to temp location owned by root, run rebuild, then clean up
                        var cmd = "printf '#!/usr/bin/env bash\\n" +
                                 "set -e\\n" +
                                 "TEMP_REPO=/tmp/nixbit-repo-$$\\n" +
                                 "echo \\\"Copying repository to temporary location...\\\"\\n" +
                                 "cp -r " + repoPath + " $TEMP_REPO\\n" +
                                 "cd $TEMP_REPO\\n" +
                                 "nixos-rebuild build --flake .#" + hostname + " -L\\n" +
                                 "echo \\\"Cleaning up temporary repository...\\\"\\n" +
                                 "rm -rf $TEMP_REPO\\n' > " + tempScript +
                                 " && chmod +x " + tempScript +
                                 " && pkexec " + tempScript +
                                 " ; rm -f " + tempScript
                        processManager.runCommand("bash", ["-c", cmd])
                    }
                    Layout.fillWidth: true
                }

                Button {
                    text: "Clone/Pull Repository"
                    icon.name: "git-clone"
                    enabled: !gitManager.isBusy && repoUrlField.text.length > 0
                    onClicked: {
                        gitManager.cloneOrPullRepository()
                    }
                    Layout.fillWidth: true
                }

                Button {
                    text: "Pull (Update)"
                    icon.name: "download"
                    enabled: !gitManager.isBusy
                    onClicked: {
                        gitManager.pullRepository()
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
                        messageTimer.restart()
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
                                    cursorPosition = text.length
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

                        Item { Layout.fillWidth: true }

                        Button {
                            text: "Clear"
                            enabled: !processManager.isRunning
                            onClicked: {
                                processManager.runCommand("clear", [])
                            }
                        }

                        Button {
                            text: "Kill Process"
                            enabled: processManager.isRunning
                            onClicked: {
                                processManager.killProcess()
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
            messageBox.text = message
            messageBox.type = success ? Kirigami.MessageType.Positive : Kirigami.MessageType.Error
            messageBox.visible = true
        }
    }
}
