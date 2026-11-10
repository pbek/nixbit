import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: generationsDialog
    title: "NixOS Generations"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 36, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 32, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    standardButtons: Dialog.Close

    onAboutToShow: {
        if (generationManager)
            generationManager.loadGenerations();
    }

    // Small rounded label, e.g. "Running"
    component Badge: Label {
        id: badge
        property color tint: Kirigami.Theme.textColor

        font: Kirigami.Theme.smallFont
        color: tint
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: 1
        bottomPadding: 1
        background: Rectangle {
            radius: height / 2
            color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, badge.tint, 0.14)
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, badge.tint, 0.4)
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.fillWidth: true
                text: generationManager && generationManager.isLoading && generationsListView.count === 0 ? "Loading generations…" : generationsListView.count === 1 ? "1 system generation" : generationsListView.count + " system generations"
                font.bold: true
                elide: Text.ElideRight
            }

            ToolButton {
                icon.name: "view-refresh"
                display: AbstractButton.IconOnly
                text: "Reload generations"
                ToolTip.visible: hovered
                ToolTip.text: text
                enabled: generationManager ? !generationManager.isLoading : false
                onClicked: {
                    if (generationManager)
                        generationManager.loadGenerations();
                }
            }
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            type: Kirigami.MessageType.Error
            text: generationManager && generationManager.error ? generationManager.error : ""
            visible: text !== ""
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
                id: generationsListView
                model: generationManager

                delegate: ItemDelegate {
                    id: delegateItem
                    required property int number
                    required property string dateTime
                    required property bool isCurrent
                    required property bool isBootDefault
                    required property string nixosVersion
                    required property string kernelVersion
                    required property string commit
                    required property string revisionLabel

                    readonly property string versions: [nixosVersion ? "NixOS " + nixosVersion : "", kernelVersion ? "Linux " + kernelVersion : ""].filter(function (part) {
                        return part !== "";
                    }).join(" · ")

                    width: ListView.view.width
                    hoverEnabled: false
                    down: false

                    // Subtle background for the running generation
                    background: Rectangle {
                        color: delegateItem.isCurrent ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.positiveTextColor, 0.08) : "transparent"
                    }

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Label {
                            Layout.alignment: Qt.AlignTop
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                            text: "#" + delegateItem.number
                            font.bold: delegateItem.isCurrent
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 0

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Label {
                                    Layout.fillWidth: true
                                    text: delegateItem.dateTime
                                    font.bold: delegateItem.isCurrent
                                    elide: Text.ElideRight
                                }

                                Badge {
                                    visible: delegateItem.isCurrent
                                    text: "Running"
                                    tint: Kirigami.Theme.positiveTextColor
                                }

                                Badge {
                                    visible: delegateItem.isBootDefault
                                    text: "Boot default"
                                    tint: delegateItem.isCurrent ? Kirigami.Theme.disabledTextColor : Kirigami.Theme.neutralTextColor
                                }

                                RevisionChip {
                                    text: delegateItem.revisionLabel
                                    commit: delegateItem.commit
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                visible: text !== ""
                                text: delegateItem.versions
                                font: Kirigami.Theme.smallFont
                                color: Kirigami.Theme.disabledTextColor
                                elide: Text.ElideRight
                            }
                        }
                    }
                }

                Kirigami.PlaceholderMessage {
                    anchors.centerIn: parent
                    width: parent.width - (Kirigami.Units.largeSpacing * 4)
                    visible: generationsListView.count === 0 && generationManager && !generationManager.isLoading
                    icon.name: "view-history"
                    text: "No generations found"
                }
            }
        }
    }
}
