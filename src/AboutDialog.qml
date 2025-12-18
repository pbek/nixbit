import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: aboutDialog
    title: "About Nixbit"
    width: 500
    height: 400
    modal: true
    standardButtons: Dialog.Close

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // Application Icon
        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "nixbit.svg"
            sourceSize.width: 128
            sourceSize.height: 128
            fillMode: Image.PreserveAspectFit
        }

        // Application Name
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Nixbit"
            font.pixelSize: 24
            font.bold: true
        }

        // Version
        Label {
            Layout.alignment: Qt.AlignHCenter
            text: "Version " + (typeof appVersion !== 'undefined' ? appVersion : "0.0.0")
            font.pixelSize: 16
            color: Kirigami.Theme.disabledTextColor
        }

        // Description
        Label {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: "A KDE Plasma application for updating NixOS systems from Git repositories."
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 14
        }

        Item {
            Layout.fillHeight: true
        }

        // Links Section
        GridLayout {
            Layout.fillWidth: true
            columns: 1
            rowSpacing: Kirigami.Units.smallSpacing

            // GitHub Repository Link
            Button {
                Layout.fillWidth: true
                icon.name: "internet-services"
                text: "View on GitHub"
                onClicked: {
                    Qt.openUrlExternally("https://github.com/pbek/nixbit");
                }
                ToolTip.visible: hovered
                ToolTip.text: "https://github.com/pbek/nixbit"
            }

            // GitHub Issues Link
            Button {
                Layout.fillWidth: true
                icon.name: "tools-report-bug"
                text: "Report an Issue"
                onClicked: {
                    Qt.openUrlExternally("https://github.com/pbek/nixbit/issues");
                }
                ToolTip.visible: hovered
                ToolTip.text: "https://github.com/pbek/nixbit/issues"
            }
        }

        // Copyright/License
        Label {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: "By Patrizio Bekerle\nLicensed under MIT License"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font.pixelSize: 12
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
