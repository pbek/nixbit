import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Dialog {
    id: settingsDialog
    title: "Settings"
    width: 600
    height: 460

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
