import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: generationsDialog
    title: "NixOS Generations"
    width: 600
    height: 400
    modal: true
    standardButtons: Dialog.Close

    onAboutToShow: {
        if (generationManager)
            generationManager.loadGenerations();
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.smallSpacing

        RowLayout {
            Layout.fillWidth: true

            Label {
                text: generationManager ? (generationManager.isLoading ? "Loading generations..." : "System generations:") : "System generations:"
                font.bold: true
            }

            Item {
                Layout.fillWidth: true
            }

            Button {
                icon.name: "view-refresh"
                text: "Refresh"
                enabled: generationManager ? !generationManager.isLoading : false
                onClicked: {
                    if (generationManager)
                        generationManager.loadGenerations();
                }
            }
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true

            ListView {
                id: generationsListView
                model: generationManager
                spacing: 2

                delegate: ItemDelegate {
                    id: delegateItem
                    width: ListView.view.width
                    highlighted: model.isCurrent

                    contentItem: RowLayout {
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: "#" + model.number
                            font.bold: model.isCurrent
                            font.pixelSize: 14
                            Layout.preferredWidth: 60
                            color: (delegateItem.hovered && !model.isCurrent) ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        Label {
                            text: model.isCurrent ? "●" : ""
                            font.bold: true
                            color: (delegateItem.hovered && !model.isCurrent) ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.positiveTextColor
                            font.pixelSize: 16
                            Layout.preferredWidth: 20
                        }

                        Label {
                            text: model.dateTime
                            font.bold: model.isCurrent
                            font.pixelSize: 14
                            Layout.fillWidth: true
                            color: (delegateItem.hovered && !model.isCurrent) ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.textColor
                        }

                        Label {
                            text: model.isCurrent ? "(current)" : ""
                            color: (delegateItem.hovered && !model.isCurrent) ? Kirigami.Theme.highlightedTextColor : Kirigami.Theme.positiveTextColor
                            font.pixelSize: 14
                            font.italic: true
                        }
                    }

                    background: Rectangle {
                        color: model.isCurrent ? Kirigami.Theme.alternateBackgroundColor : (parent.hovered ? Kirigami.Theme.hoverColor : "transparent")
                        radius: 3
                    }
                }
            }
        }

        Label {
            text: generationManager && generationManager.error ? generationManager.error : ""
            visible: generationManager && generationManager.error !== ""
            color: Kirigami.Theme.negativeTextColor
            wrapMode: Text.Wrap
            Layout.fillWidth: true
        }
    }
}
