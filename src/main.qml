import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root
    title: "NixBit - Git Repository Manager"
    width: 800
    height: 600

    pageStack.initialPage: Kirigami.Page {
        title: "Repository Manager"

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.largeSpacing

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

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

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

            GroupBox {
                title: "Information"
                Layout.fillWidth: true
                Layout.fillHeight: true

                ScrollView {
                    anchors.fill: parent

                    TextArea {
                        readOnly: true
                        wrapMode: TextArea.WordWrap
                        text: "This application manages a Git repository in your application data directory.\n\n" +
                              "Default repository: https://github.com/pbek/nixcfg.git\n\n" +
                              "You can change the repository URL above. The repository will be cloned to:\n" +
                              gitManager.localPath + "\n\n" +
                              "Use 'Clone/Pull Repository' to clone a new repository or pull updates if it already exists.\n" +
                              "Use 'Pull (Update)' to update an existing repository."
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
