import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

Dialog {
    id: commitsDialog
    title: "Available Updates"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 40, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 32, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    standardButtons: Dialog.Close

    readonly property int commitCount: commitListView.count

    function commitUrl(sha) {
        return Utils.commitUrl(gitManager ? gitManager.repositoryUrl : "", sha);
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        // Header information
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.fillWidth: true
                text: commitsDialog.commitCount > 0 ? (commitsDialog.commitCount === 1 ? "1 new commit is available:" : commitsDialog.commitCount + " new commits are available:") : "Your configuration is up to date."
                font.bold: true
                elide: Text.ElideRight
            }

            Button {
                text: "Pull Without Rebuilding"
                icon.name: "download"
                enabled: gitManager ? !gitManager.isBusy && gitManager.commitsBehind > 0 : false
                onClicked: {
                    if (gitManager)
                        gitManager.ignoreUpdate();
                }
                ToolTip.visible: hovered
                ToolTip.text: "Pull the commits so they are no longer listed here.\nThe changes will be applied with the next build."
                ToolTip.delay: Kirigami.Units.toolTipDelay
            }

            ToolButton {
                icon.name: "view-refresh"
                display: AbstractButton.IconOnly
                text: "Check for updates"
                ToolTip.visible: hovered
                ToolTip.text: text
                enabled: gitManager ? !gitManager.isBusy : false
                onClicked: {
                    if (gitManager)
                        gitManager.checkForUpdates();
                }
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
                id: commitListView
                model: gitManager ? gitManager.commitsBehindList : []
                reuseItems: true

                delegate: ItemDelegate {
                    id: delegateItem
                    required property var modelData
                    required property int index
                    readonly property string url: commitsDialog.commitUrl(modelData.sha)

                    width: ListView.view.width
                    hoverEnabled: url !== ""
                    down: false
                    onClicked: {
                        if (url !== "")
                            Qt.openUrlExternally(url);
                    }

                    ToolTip.visible: hovered && url !== ""
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: "Open commit on " + Utils.forgeName(gitManager ? gitManager.repositoryUrl : "")

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            Layout.fillWidth: true
                            text: delegateItem.modelData.message || ""
                            wrapMode: Text.Wrap
                            maximumLineCount: 4
                            elide: Text.ElideRight
                        }

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Label {
                                text: delegateItem.modelData.sha || ""
                                font.family: Kirigami.Theme.fixedWidthFont.family
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                                color: delegateItem.url !== "" ? Kirigami.Theme.linkColor : Kirigami.Theme.disabledTextColor
                            }

                            Label {
                                Layout.fillWidth: true
                                text: "· " + (delegateItem.modelData.author || "")
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                            }

                            Label {
                                text: delegateItem.modelData.dateRelative || ""
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor

                                HoverHandler {
                                    id: dateHover
                                }

                                ToolTip.visible: dateHover.hovered && (delegateItem.modelData.date || "") !== ""
                                ToolTip.text: delegateItem.modelData.date || ""
                            }
                        }
                    }

                    Kirigami.Separator {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        visible: delegateItem.index < commitListView.count - 1
                    }
                }

                // Empty state
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: commitListView.count === 0
                    icon.name: "emblem-success"
                    text: "No updates available"
                    explanation: "Your system is up to date with the remote repository."
                }
            }
        }
    }
}
