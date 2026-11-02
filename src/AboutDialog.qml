import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Dialog {
    id: aboutDialog
    title: "About Nixbit"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 26, parent.width - Kirigami.Units.gridUnit * 2)
    modal: true
    standardButtons: Dialog.Close

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // Application Icon
        Image {
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.largeSpacing
            source: "nixbit.svg"
            sourceSize.width: Kirigami.Units.iconSizes.enormous
            sourceSize.height: Kirigami.Units.iconSizes.enormous
            fillMode: Image.PreserveAspectFit
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 0

            Kirigami.Heading {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "Nixbit"
                level: 1
            }

            Label {
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
                text: "Version " + (typeof appVersion !== 'undefined' ? appVersion : "0.0.0")
                color: Kirigami.Theme.disabledTextColor
            }
        }

        Label {
            Layout.fillWidth: true
            text: "A KDE Plasma application for updating NixOS systems from Git repositories."
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
        }

        RowLayout {
            Layout.alignment: Qt.AlignHCenter
            spacing: Kirigami.Units.smallSpacing

            Button {
                icon.name: "internet-services"
                text: "Website"
                onClicked: Qt.openUrlExternally("https://github.com/pbek/nixbit")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay
                ToolTip.text: "https://github.com/pbek/nixbit"
            }

            Button {
                icon.name: "tools-report-bug"
                text: "Report an Issue"
                onClicked: Qt.openUrlExternally("https://github.com/pbek/nixbit/issues")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay
                ToolTip.text: "https://github.com/pbek/nixbit/issues"
            }
        }

        Label {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.smallSpacing
            text: "© Patrizio Bekerle · MIT License"
            wrapMode: Text.WordWrap
            horizontalAlignment: Text.AlignHCenter
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
