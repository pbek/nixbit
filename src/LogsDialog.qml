import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: logsDialog
    title: "Build Logs"
    width: 800
    height: 500
    modal: true
    standardButtons: Dialog.Close

    // Helper function to format file size
    function formatFileSize(bytes) {
        if (bytes < 1024)
            return bytes + " B";
        else if (bytes < 1024 * 1024)
            return (bytes / 1024).toFixed(1) + " KB";
        else if (bytes < 1024 * 1024 * 1024)
            return (bytes / (1024 * 1024)).toFixed(1) + " MB";
        else
            return (bytes / (1024 * 1024 * 1024)).toFixed(1) + " GB";
    }

    onAboutToShow: {
        if (logManager && settingsManager) {
            logManager.refreshLogs(settingsManager.getLogDirectory());
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: logManager && logManager.logFiles.length > 0 ? "Build logs (" + logManager.logFiles.length + "):" : "Build logs:"
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                icon.name: "view-refresh"
                text: "Refresh"
                onClicked: {
                    if (logManager && settingsManager) {
                        logManager.refreshLogs(settingsManager.getLogDirectory());
                    }
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: logsListView
                model: logManager ? logManager.logFiles : []
                spacing: 2

                delegate: ItemDelegate {
                    id: delegateItem
                    width: ListView.view.width

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        // Status indicator
                        Label {
                            text: modelData.exitCode === 0 ? "✓" : "✗"
                            font.bold: true
                            font.pixelSize: 16
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : (modelData.exitCode === 0 ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor)
                            Layout.preferredWidth: 30
                        }

                        // Build type badge
                        Label {
                            text: modelData.buildType ? modelData.buildType.toUpperCase() : "BUILD"
                            font.pixelSize: 10
                            font.bold: true
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.backgroundColor
                            padding: 4
                            background: Rectangle {
                                color: delegateItem.hovered ? Kirigami.Theme.highlightColor : (modelData.buildType === "switch" ? Kirigami.Theme.focusColor : Kirigami.Theme.neutralTextColor)
                                radius: 3
                            }
                            Layout.preferredWidth: 55
                        }

                        // Timestamp
                        Label {
                            text: modelData.displayName
                            font.pixelSize: 14
                            Layout.fillWidth: true
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        // File size
                        Label {
                            text: logsDialog.formatFileSize(modelData.fileSize || 0)
                            font.pixelSize: 12
                            font.italic: true
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                            Layout.preferredWidth: 70
                        }

                        // Status text
                        Label {
                            text: modelData.exitCode === 0 ? "Success" : "Failed (exit " + modelData.exitCode + ")"
                            font.pixelSize: 12
                            font.italic: true
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : (modelData.exitCode === 0 ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.negativeTextColor)
                            Layout.preferredWidth: 120
                        }

                        // Open button
                        Button {
                            icon.name: "document-open"
                            text: "Open"
                            flat: true
                            onClicked: {
                                if (logManager)
                                    logManager.openLogInEditor(modelData.filePath);
                            }
                        }

                        // Delete button
                        Button {
                            icon.name: "edit-delete"
                            display: AbstractButton.IconOnly
                            flat: true
                            ToolTip.visible: hovered
                            ToolTip.text: "Delete log"
                            onClicked: {
                                deleteLogDialog.logToDelete = modelData.filePath;
                                deleteLogDialog.logFileName = modelData.displayName;
                                deleteLogDialog.open();
                            }
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered ? Kirigami.Theme.hoverColor : "transparent"
                        radius: 3
                    }
                }

                // Placeholder when no logs
                Label {
                    anchors.centerIn: parent
                    visible: logsListView.count === 0
                    text: "No build logs available yet"
                    font.italic: true
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }

    // Delete confirmation dialog
    Dialog {
        id: deleteLogDialog
        title: "Delete Log File"
        modal: true
        standardButtons: Dialog.Ok | Dialog.Cancel
        property string logToDelete: ""
        property string logFileName: ""

        Label {
            text: "Are you sure you want to delete this log file?\n\n" + deleteLogDialog.logFileName
            wrapMode: Text.WordWrap
        }

        onAccepted: {
            if (logManager && logToDelete !== "") {
                logManager.deleteLog(logToDelete);
            }
        }
    }
}
