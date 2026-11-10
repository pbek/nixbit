import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

// Short commit hash in a subtle monospace chip, opens the commit on the Git
// forge when clicked
Label {
    id: chip

    // Full commit hash, used for the link
    property string commit
    readonly property string forgeName: Utils.forgeName(gitManager ? gitManager.repositoryUrl : "")
    readonly property string url: Utils.commitUrl(gitManager ? gitManager.repositoryUrl : "", commit)
    readonly property bool linked: url !== ""
    readonly property bool hovered: chipHover.hovered
    readonly property bool highlighted: linked && (chipHover.hovered || activeFocus)

    function open() {
        if (linked)
            Qt.openUrlExternally(url);
    }

    visible: text !== ""
    font.family: "monospace"
    font.pointSize: Kirigami.Theme.smallFont.pointSize
    font.underline: highlighted
    color: highlighted ? Kirigami.Theme.linkColor : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.75)
    leftPadding: Kirigami.Units.smallSpacing
    rightPadding: Kirigami.Units.smallSpacing
    topPadding: 1
    bottomPadding: 1
    activeFocusOnTab: linked
    Accessible.role: linked ? Accessible.Link : Accessible.StaticText
    Accessible.name: linked ? "Open commit " + text + " on " + forgeName : "Commit " + text

    Keys.onReturnPressed: open()
    Keys.onSpacePressed: open()

    background: Rectangle {
        radius: Kirigami.Units.cornerRadius
        color: chip.highlighted ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.linkColor, 0.12) : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.07)
        border.color: chip.highlighted ? Kirigami.Theme.linkColor : Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.18)

        Behavior on color {
            ColorAnimation {
                duration: Kirigami.Units.shortDuration
            }
        }
    }

    HoverHandler {
        id: chipHover
        cursorShape: chip.linked ? Qt.PointingHandCursor : Qt.ArrowCursor
    }

    TapHandler {
        enabled: chip.linked
        onTapped: chip.open()
    }

    ToolTip.visible: chipHover.hovered
    ToolTip.delay: Kirigami.Units.toolTipDelay
    ToolTip.text: linked ? "Open commit " + text + " on " + forgeName : "Commit " + text
}
