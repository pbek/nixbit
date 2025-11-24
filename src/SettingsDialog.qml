import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: settingsDialog
    title: "Settings"
    width: 800
    height: 540

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Kirigami.FormLayout {
            Layout.fillWidth: true

            RowLayout {
                Layout.leftMargin: Kirigami.Units.largeSpacing
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
        }

        // Build Hosts Management Section
        Kirigami.Separator {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing
        }

        Label {
            text: "Build Hosts"
            font.bold: true
            font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
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
            Layout.preferredHeight: 150

            ListView {
                id: buildHostsList
                model: settingsManager ? settingsManager.buildHosts : []
                clip: true
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
                }
            }
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            TextField {
                Kirigami.FormData.label: "Build Host:"
                text: settingsManager ? settingsManager.buildHost : ""
                placeholderText: "user@hostname (optional)"
                onEditingFinished: {
                    if (settingsManager)
                        settingsManager.buildHost = text;
                }
                ToolTip.visible: hovered
                ToolTip.text: "Remote host for building (e.g., user@buildserver). Leave empty to build locally."
                visible: false  // Hide old field, keeping for backwards compatibility
            }
        }

        Item {
            Layout.fillHeight: true
        }

        RowLayout {
            Layout.alignment: Qt.AlignRight

            Button {
                text: "Close"
                onClicked: settingsDialog.close()
            }
        }
    }
}
