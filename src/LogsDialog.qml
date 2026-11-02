import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: logsDialog
    title: "Build Logs"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 40, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 30, parent.height - Kirigami.Units.gridUnit * 2)
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

    function refresh() {
        if (logManager && settingsManager) {
            logManager.refreshLogs(settingsManager.getLogDirectory());
        }
    }

    onAboutToShow: refresh()

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.fillWidth: true
                text: logsListView.count === 1 ? "1 stored log" : logsListView.count + " stored logs"
                font.bold: true
                elide: Text.ElideRight
            }

            ToolButton {
                icon.name: "document-open-folder"
                display: AbstractButton.IconOnly
                text: "Open log folder"
                ToolTip.visible: hovered
                ToolTip.text: text
                enabled: settingsManager !== null && processManager !== null
                onClicked: processManager.startDetached("xdg-open", [settingsManager.getLogDirectory()])
            }

            ToolButton {
                icon.name: "view-refresh"
                display: AbstractButton.IconOnly
                text: "Reload logs"
                ToolTip.visible: hovered
                ToolTip.text: text
                onClicked: logsDialog.refresh()
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            Component.onCompleted: {
                if (background) {
                    background.visible = true;
                }
            }

            ListView {
                id: logsListView
                model: logManager ? logManager.logFiles : []

                delegate: ItemDelegate {
                    id: delegateItem
                    required property var modelData
                    readonly property bool succeeded: modelData.exitCode === 0
                    readonly property string buildType: modelData.buildType || "build"

                    width: ListView.view.width
                    onClicked: {
                        if (logManager)
                            logManager.openLogInEditor(modelData.filePath);
                    }

                    ToolTip.visible: hovered && !deleteButton.hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: "Open log in text editor"

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Kirigami.Icon {
                            source: delegateItem.succeeded ? "emblem-success" : "emblem-error"
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium
                            implicitHeight: Kirigami.Units.iconSizes.smallMedium
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            Label {
                                Layout.fillWidth: true
                                text: delegateItem.modelData.displayName
                                elide: Text.ElideRight
                            }

                            Label {
                                Layout.fillWidth: true
                                text: (delegateItem.succeeded ? "Succeeded" : "Failed with exit code " + delegateItem.modelData.exitCode) + " · " + logsDialog.formatFileSize(delegateItem.modelData.fileSize || 0)
                                font: Kirigami.Theme.smallFont
                                color: delegateItem.succeeded ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.negativeTextColor
                                elide: Text.ElideRight
                            }
                        }

                        // Build type badge
                        Label {
                            text: delegateItem.buildType.charAt(0).toUpperCase() + delegateItem.buildType.slice(1)
                            font: Kirigami.Theme.smallFont
                            horizontalAlignment: Text.AlignHCenter
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 3.5
                            leftPadding: Kirigami.Units.smallSpacing
                            rightPadding: Kirigami.Units.smallSpacing
                            topPadding: Kirigami.Units.smallSpacing / 2
                            bottomPadding: Kirigami.Units.smallSpacing / 2
                            color: Kirigami.Theme.textColor
                            background: Rectangle {
                                readonly property color tint: delegateItem.buildType === "switch" ? Kirigami.Theme.positiveTextColor : delegateItem.buildType === "boot" ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.linkColor
                                color: Qt.rgba(tint.r, tint.g, tint.b, 0.15)
                                border.color: Qt.rgba(tint.r, tint.g, tint.b, 0.5)
                                border.width: 1
                                radius: height / 2
                            }
                        }

                        ToolButton {
                            id: deleteButton
                            icon.name: "edit-delete"
                            display: AbstractButton.IconOnly
                            text: "Delete log"
                            ToolTip.visible: hovered
                            ToolTip.text: text
                            onClicked: {
                                deleteLogDialog.logToDelete = delegateItem.modelData.filePath;
                                deleteLogDialog.logFileName = delegateItem.modelData.displayName;
                                deleteLogDialog.open();
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: logsListView.count === 0
                    icon.name: "documentation"
                    text: "No build logs yet"
                    explanation: "A log is saved every time a build, switch or boot finishes."
                }
            }
        }
    }

    // Delete confirmation dialog
    Kirigami.PromptDialog {
        id: deleteLogDialog
        parent: Overlay.overlay
        title: "Delete Log?"
        subtitle: "The log from " + logFileName + " will be permanently deleted."
        dialogType: Kirigami.PromptDialog.Warning
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: "Delete"
                icon.name: "edit-delete"
                onTriggered: deleteLogDialog.accept()
            }
        ]
        property string logToDelete: ""
        property string logFileName: ""

        onAccepted: {
            if (logManager && logToDelete !== "") {
                logManager.deleteLog(logToDelete);
            }
        }
    }
}
