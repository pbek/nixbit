import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: commitsDialog
    title: "Available Updates"
    width: 800
    height: 600
    modal: true
    standardButtons: Dialog.Close

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        // Header information
        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.largeSpacing

            Label {
                text: "Commits Behind:"
                font.bold: true
            }

            Label {
                text: gitManager ? gitManager.commitsBehind.toString() : "0"
                font.bold: true
                color: Kirigami.Theme.neutralTextColor
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.2
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                text: "Ignore commits"
                icon.name: "download"
                enabled: gitManager ? !gitManager.isBusy && gitManager.commitsBehind > 0 : false
                onClicked: {
                    if (gitManager)
                        gitManager.ignoreUpdate();
                }
                ToolTip.visible: hovered
                ToolTip.text: "Pulls changes from git to remove them from this list.\nNote: This doesn't actually ignore commits, it downloads them. Changes will be applied in the next build."
                ToolTip.delay: 500
            }

            Button {
                text: "Refresh"
                icon.name: "view-refresh"
                enabled: gitManager ? !gitManager.isBusy : false
                onClicked: {
                    if (gitManager)
                        gitManager.checkForUpdates();
                }
            }
        }

        Kirigami.Separator {
            Layout.fillWidth: true
        }

        // Commits list
        Label {
            text: gitManager && gitManager.commitsBehindList && gitManager.commitsBehindList.length > 0 ? "The following commits are available for update:" : "No commits behind. Your system is up to date!"
            font.italic: true
            color: Kirigami.Theme.disabledTextColor
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: commitListView
                model: gitManager ? gitManager.commitsBehindList : []
                spacing: Kirigami.Units.smallSpacing

                delegate: ItemDelegate {
                    id: delegateItem
                    width: ListView.view.width

                    contentItem: ColumnLayout {
                        spacing: Kirigami.Units.smallSpacing

                        // Header row with SHA, author, and date
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.largeSpacing

                            // SHA - clickable if GitHub repo
                            Label {
                                text: {
                                    var sha = modelData.sha || "";
                                    var repoUrl = gitManager ? gitManager.repositoryUrl : "";
                                    var isGitHub = repoUrl.includes("github.com");

                                    if (isGitHub && sha) {
                                        return '<a href="' + getGitHubCommitUrl(repoUrl, sha) + '">' + sha + '</a>';
                                    }
                                    return sha;
                                }
                                textFormat: Text.RichText
                                font.family: "monospace"
                                font.bold: true
                                color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.linkColor
                                Layout.preferredWidth: 70

                                onLinkActivated: function (link) {
                                    Qt.openUrlExternally(link);
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                                    acceptedButtons: Qt.NoButton
                                }

                                function getGitHubCommitUrl(repoUrl, sha) {
                                    // Convert various GitHub URL formats to web commit URL
                                    var url = repoUrl;

                                    // Handle git@ SSH format: git@github.com:user/repo.git
                                    if (url.startsWith("git@github.com:")) {
                                        url = url.replace("git@github.com:", "https://github.com/");
                                    }

                                    // Handle https format: https://github.com/user/repo.git
                                    // Remove .git suffix if present
                                    if (url.endsWith(".git")) {
                                        url = url.substring(0, url.length - 4);
                                    }

                                    // Ensure it starts with https://
                                    if (!url.startsWith("http")) {
                                        url = "https://github.com/" + url;
                                    }

                                    return url + "/commit/" + sha;
                                }
                            }

                            // Author
                            Label {
                                text: modelData.author || ""
                                color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                                font.bold: true
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                            }

                            // Relative date
                            Label {
                                text: modelData.dateRelative || ""
                                color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }

                        // Commit message
                        Label {
                            Layout.fillWidth: true
                            text: modelData.message || ""
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                            wrapMode: Text.Wrap
                        }

                        // Full date (smaller, less prominent)
                        Label {
                            Layout.fillWidth: true
                            text: "Committed: " + (modelData.date || "")
                            color: delegateItem.hovered ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.disabledTextColor
                            font.pointSize: Kirigami.Theme.smallFont.pointSize * 0.9
                        }
                    }

                    background: Rectangle {
                        color: parent.hovered ? Kirigami.Theme.hoverColor : Kirigami.Theme.backgroundColor
                        border.color: Kirigami.Theme.separatorColor
                        border.width: 1
                        radius: Kirigami.Units.smallSpacing
                    }
                }

                // Empty state
                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: commitListView.count === 0
                    icon.name: "checkmark"
                    text: "No updates available"
                    explanation: "Your system is up to date with the remote repository."
                }
            }
        }
    }
}
