import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: generationsDialog
    title: "NixOS Generations"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 30, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 30, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    standardButtons: Dialog.Close

    onAboutToShow: {
        if (generationManager)
            generationManager.loadGenerations();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Label {
                Layout.fillWidth: true
                text: generationManager && generationManager.isLoading ? "Loading generations…" : generationsListView.count === 1 ? "1 system generation" : generationsListView.count + " system generations"
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

                    width: ListView.view.width
                    hoverEnabled: false
                    down: false
                    highlighted: isCurrent

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.largeSpacing

                        Label {
                            text: "#" + delegateItem.number
                            font.bold: delegateItem.isCurrent
                            Layout.minimumWidth: Kirigami.Units.gridUnit * 3
                            color: delegateItem.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        Label {
                            text: delegateItem.dateTime
                            font.bold: delegateItem.isCurrent
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            color: delegateItem.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        Label {
                            visible: delegateItem.isCurrent
                            text: "Current"
                            font: Kirigami.Theme.smallFont
                            color: delegateItem.highlighted ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.positiveTextColor
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
