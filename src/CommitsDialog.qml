import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

// Lists the commits that are not pulled yet, so the user can decide whether
// to update
Dialog {
    id: commitsDialog
    title: "Available Updates"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 46, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 38, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    padding: Kirigami.Units.largeSpacing

    // Text, tooltip and state of the update action of the main window
    property string updateText: "Update System"
    property string updateToolTip: ""
    property bool canUpdate: true
    signal updateRequested

    readonly property string repositoryUrl: gitManager ? gitManager.repositoryUrl : ""
    readonly property string forgeName: Utils.forgeName(repositoryUrl)
    readonly property string repositoryPath: Utils.repositoryPath(repositoryUrl)
    readonly property bool busy: gitManager ? gitManager.isBusy : false
    readonly property var commits: gitManager ? gitManager.commitsBehindList : []
    readonly property int totalCount: gitManager ? Math.max(gitManager.commitsBehind, commits.length) : commits.length
    readonly property string compareUrl: gitManager ? Utils.compareUrl(repositoryUrl, gitManager.localRevision, gitManager.latestFetchedRevision) : ""
    readonly property bool filtering: searchField.text.trim() !== ""

    // Current time for relative dates, refreshed while the dialog is open
    property double now: Date.now()

    // Totals over all listed commits
    readonly property var totals: {
        var authors = {};
        var insertions = 0;
        var deletions = 0;
        for (var i = 0; i < commits.length; i++) {
            authors[commits[i].author] = true;
            insertions += Math.max(0, commits[i].insertions || 0);
            deletions += Math.max(0, commits[i].deletions || 0);
        }
        return {
            authors: Object.keys(authors).length,
            insertions: insertions,
            deletions: deletions
        };
    }

    function commitUrl(sha) {
        return Utils.commitUrl(repositoryUrl, sha);
    }

    function dayLabel(timestamp) {
        var date = new Date(timestamp);
        var today = new Date(now);
        var startOfToday = new Date(today.getFullYear(), today.getMonth(), today.getDate()).getTime();
        var startOfDay = new Date(date.getFullYear(), date.getMonth(), date.getDate()).getTime();
        var days = Math.round((startOfToday - startOfDay) / 86400000);
        if (days <= 0)
            return "Today";
        if (days === 1)
            return "Yesterday";
        if (days < 7)
            return Qt.formatDate(date, "dddd");
        return Qt.formatDate(date, today.getFullYear() === date.getFullYear() ? "dddd, d MMMM" : "d MMMM yyyy");
    }

    function rebuildModel() {
        var query = searchField.text.trim().toLowerCase();
        commitModel.clear();
        for (var i = 0; i < commits.length; i++) {
            var commit = commits[i];
            var sha = commit.sha || "";
            if (query !== "") {
                var haystack = [commit.message, commit.body, commit.author, commit.authorEmail, sha].join("\n").toLowerCase();
                if (haystack.indexOf(query) === -1)
                    continue;
            }
            commitModel.append({
                sha: sha,
                shortSha: commit.shortSha || sha.substring(0, 7),
                summary: commit.message || "",
                body: commit.body || "",
                author: commit.author || "",
                authorEmail: commit.authorEmail || "",
                date: commit.date || "",
                timestamp: commit.timestamp || 0,
                day: commit.timestamp ? dayLabel(commit.timestamp) : "",
                isMerge: (commit.parentCount || 0) > 1,
                filesChanged: commit.filesChanged !== undefined ? commit.filesChanged : -1,
                insertions: commit.insertions !== undefined ? commit.insertions : -1,
                deletions: commit.deletions !== undefined ? commit.deletions : -1,
                expanded: false
            });
        }
    }

    function copyToClipboard(text) {
        clipboardHelper.text = text;
        clipboardHelper.selectAll();
        clipboardHelper.copy();
        clipboardHelper.text = "";
    }

    onAboutToShow: {
        now = Date.now();
        searchField.clear();
        rebuildModel();
        commitListView.positionViewAtBeginning();
    }

    onCommitsChanged: rebuildModel()
    onAccepted: updateRequested()

    Timer {
        interval: 60000
        repeat: true
        running: commitsDialog.visible
        onTriggered: commitsDialog.now = Date.now()
    }

    ListModel {
        id: commitModel
    }

    // Used to put text on the clipboard
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: commitsDialog.visible
        onActivated: searchField.forceActiveFocus()
    }

    // Small rounded label, e.g. "Merge"
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

    // Circle with the initials of the author, colored by name
    component AuthorAvatar: Rectangle {
        id: avatar
        property string name

        readonly property string initials: {
            var words = name.replace(/\[bot\]$/, "").split(/[\s._-]+/).filter(function (word) {
                return word !== "";
            });
            if (words.length === 0)
                return "?";
            var first = words[0].charAt(0);
            var last = words.length > 1 ? words[words.length - 1].charAt(0) : "";
            return (first + last).toUpperCase();
        }
        readonly property real hue: {
            var hash = 0;
            for (var i = 0; i < name.length; i++)
                hash = (hash * 31 + name.charCodeAt(i)) % 360;
            return hash / 360;
        }

        implicitWidth: Kirigami.Units.iconSizes.medium
        implicitHeight: implicitWidth
        radius: width / 2
        color: Qt.hsla(hue, 0.45, Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark ? 0.38 : 0.55, 1)
        Accessible.ignored: true

        Label {
            anchors.centerIn: parent
            text: avatar.initials
            color: "white"
            font.pixelSize: avatar.height * 0.4
            font.weight: Font.DemiBold
        }
    }

    // Lines added and removed, e.g. "+12 −3"
    component LineStats: Row {
        property int insertions: -1
        property int deletions: -1
        property font font: Kirigami.Theme.smallFont

        spacing: Kirigami.Units.smallSpacing
        visible: insertions >= 0 && deletions >= 0

        Label {
            text: "+" + parent.insertions
            font: parent.font
            color: Kirigami.Theme.positiveTextColor
        }

        Label {
            text: "−" + parent.deletions
            font: parent.font
            color: Kirigami.Theme.negativeTextColor
        }
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        // Summary of the available updates
        Rectangle {
            id: summaryCard
            readonly property color tint: commitsDialog.totalCount > 0 ? Kirigami.Theme.neutralTextColor : Kirigami.Theme.positiveTextColor

            Layout.fillWidth: true
            implicitHeight: summaryLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
            radius: Kirigami.Units.cornerRadius * 2
            color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, tint, 0.07)
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, tint, 0.3)

            Behavior on color {
                ColorAnimation {
                    duration: Kirigami.Units.longDuration
                }
            }

            RowLayout {
                id: summaryLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                Rectangle {
                    Layout.alignment: Qt.AlignTop
                    implicitWidth: Kirigami.Units.iconSizes.large + Kirigami.Units.smallSpacing * 2
                    implicitHeight: implicitWidth
                    radius: Kirigami.Units.cornerRadius * 2
                    color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, summaryCard.tint, 0.16)

                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Kirigami.Units.iconSizes.medium
                        height: width
                        source: commitsDialog.totalCount > 0 ? "system-software-update" : "emblem-success"
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        Layout.fillWidth: true
                        level: 2
                        text: commitsDialog.totalCount > 0 ? (commitsDialog.totalCount === 1 ? "1 new commit" : commitsDialog.totalCount + " new commits") : commitsDialog.busy ? "Checking for updates…" : "You're up to date"
                        elide: Text.ElideRight
                    }

                    Label {
                        Layout.fillWidth: true
                        text: {
                            var source = commitsDialog.repositoryPath ? commitsDialog.repositoryPath + " on " + commitsDialog.forgeName : "the remote repository";
                            if (commitsDialog.totalCount === 0)
                                return "The local repository has all commits of " + source + ".";
                            var newest = commitsDialog.commits.length > 0 && commitsDialog.commits[0].timestamp ? ", the newest from " + Utils.relativeTime(commitsDialog.commits[0].timestamp, commitsDialog.now) : "";
                            return "Not pulled yet from " + source + newest + ". Review them before you update.";
                        }
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        color: Kirigami.Theme.disabledTextColor
                    }

                    // Totals over all listed commits
                    Flow {
                        Layout.fillWidth: true
                        Layout.topMargin: Kirigami.Units.smallSpacing
                        visible: commitsDialog.commits.length > 0
                        spacing: Kirigami.Units.largeSpacing

                        Row {
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Kirigami.Units.iconSizes.small
                                height: width
                                source: "user-identity"
                                color: Kirigami.Theme.disabledTextColor
                            }

                            Label {
                                text: commitsDialog.totals.authors === 1 ? "1 author" : commitsDialog.totals.authors + " authors"
                                font: Kirigami.Theme.smallFont
                            }
                        }

                        Row {
                            spacing: Kirigami.Units.smallSpacing

                            HoverHandler {
                                id: totalsHover
                            }

                            ToolTip.visible: totalsHover.hovered
                            ToolTip.delay: Kirigami.Units.toolTipDelay
                            ToolTip.text: "Lines added and removed, summed up over all commits"

                            Kirigami.Icon {
                                anchors.verticalCenter: parent.verticalCenter
                                width: Kirigami.Units.iconSizes.small
                                height: width
                                source: "document-edit"
                                color: Kirigami.Theme.disabledTextColor
                            }

                            LineStats {
                                anchors.verticalCenter: parent.verticalCenter
                                insertions: commitsDialog.totals.insertions
                                deletions: commitsDialog.totals.deletions
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.alignment: Qt.AlignTop
                    spacing: Kirigami.Units.smallSpacing

                    RowLayout {
                        Layout.alignment: Qt.AlignRight
                        spacing: 0

                        BusyIndicator {
                            implicitWidth: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
                            implicitHeight: implicitWidth
                            padding: Kirigami.Units.smallSpacing
                            running: commitsDialog.busy
                            visible: running
                        }

                        ToolButton {
                            icon.name: "view-refresh"
                            display: AbstractButton.IconOnly
                            text: "Check for updates"
                            visible: !commitsDialog.busy
                            enabled: gitManager !== null
                            ToolTip.visible: hovered
                            ToolTip.delay: Kirigami.Units.toolTipDelay
                            ToolTip.text: text
                            onClicked: gitManager.checkForUpdates()
                        }
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        visible: commitsDialog.compareUrl !== "" && commitsDialog.totalCount > 0
                        text: "Compare"
                        icon.name: "globe"
                        flat: true
                        ToolTip.visible: hovered
                        ToolTip.delay: Kirigami.Units.toolTipDelay
                        ToolTip.text: "Show the combined changes of all new commits on " + commitsDialog.forgeName
                        onClicked: Qt.openUrlExternally(commitsDialog.compareUrl)
                    }
                }
            }
        }

        Kirigami.SearchField {
            id: searchField
            Layout.fillWidth: true
            visible: commitsDialog.commits.length > 1
            placeholderText: "Filter by message, author or commit hash…"
            onTextChanged: commitsDialog.rebuildModel()
        }

        Kirigami.InlineMessage {
            Layout.fillWidth: true
            visible: commitsDialog.commits.length > 0 && commitsDialog.totalCount > commitsDialog.commits.length
            type: Kirigami.MessageType.Information
            text: "Only the newest " + commitsDialog.commits.length + " of " + commitsDialog.totalCount + " commits are listed."
            actions: [
                Kirigami.Action {
                    visible: commitsDialog.compareUrl !== ""
                    text: "Compare on " + commitsDialog.forgeName
                    icon.name: "globe"
                    onTriggered: Qt.openUrlExternally(commitsDialog.compareUrl)
                }
            ]
        }

        // Commit list
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Kirigami.Theme.colorSet: Kirigami.Theme.View
            Kirigami.Theme.inherit: false
            color: Kirigami.Theme.backgroundColor
            radius: Kirigami.Units.cornerRadius
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)

            ScrollView {
                anchors.fill: parent
                anchors.margins: 1
                clip: true

                ListView {
                    id: commitListView
                    model: commitModel
                    activeFocusOnTab: true
                    keyNavigationEnabled: true
                    currentIndex: -1
                    boundsBehavior: Flickable.StopAtBounds

                    section.property: "day"
                    section.criteria: ViewSection.FullString
                    section.labelPositioning: ViewSection.InlineLabels | ViewSection.CurrentLabelAtStart
                    section.delegate: Rectangle {
                        id: sectionHeader
                        required property string section

                        width: ListView.view.width
                        implicitHeight: sectionLabel.implicitHeight + Kirigami.Units.smallSpacing * 2
                        color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.04)

                        Label {
                            id: sectionLabel
                            anchors.verticalCenter: parent.verticalCenter
                            x: Kirigami.Units.largeSpacing
                            text: sectionHeader.section
                            font: Kirigami.Theme.smallFont
                            color: Kirigami.Theme.disabledTextColor
                        }

                        Kirigami.Separator {
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.bottom: parent.bottom
                        }
                    }

                    delegate: ItemDelegate {
                        id: delegateItem
                        required property int index
                        required property string sha
                        required property string shortSha
                        required property string summary
                        required property string body
                        required property string author
                        required property string authorEmail
                        required property string date
                        required property double timestamp
                        required property bool isMerge
                        required property int filesChanged
                        required property int insertions
                        required property int deletions
                        required property bool expanded

                        readonly property string url: commitsDialog.commitUrl(sha)
                        readonly property bool hasBody: body !== ""

                        function toggle() {
                            if (hasBody)
                                commitModel.setProperty(index, "expanded", !expanded);
                        }

                        width: ListView.view.width
                        padding: Kirigami.Units.largeSpacing
                        topPadding: Kirigami.Units.mediumSpacing
                        bottomPadding: Kirigami.Units.mediumSpacing
                        hoverEnabled: true
                        down: false
                        highlighted: ListView.isCurrentItem && commitListView.activeFocus
                        Accessible.name: summary + ", " + author + ", " + Utils.relativeTime(timestamp, commitsDialog.now)

                        onClicked: toggle()
                        Keys.onReturnPressed: url !== "" ? Qt.openUrlExternally(url) : toggle()
                        Keys.onSpacePressed: toggle()

                        background: Rectangle {
                            color: delegateItem.highlighted ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.18) : delegateItem.hovered ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.08) : "transparent"

                            Behavior on color {
                                ColorAnimation {
                                    duration: Kirigami.Units.shortDuration
                                }
                            }

                            Kirigami.Separator {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                visible: delegateItem.index < commitListView.count - 1
                                opacity: 0.6
                            }
                        }

                        contentItem: RowLayout {
                            spacing: Kirigami.Units.largeSpacing

                            AuthorAvatar {
                                Layout.alignment: Qt.AlignTop
                                Layout.topMargin: 2
                                name: delegateItem.author
                            }

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    Label {
                                        Layout.fillWidth: true
                                        text: delegateItem.summary
                                        textFormat: Text.PlainText
                                        font.weight: Font.DemiBold
                                        wrapMode: Text.Wrap
                                        maximumLineCount: delegateItem.expanded ? 10 : 2
                                        elide: Text.ElideRight
                                    }

                                    Badge {
                                        Layout.alignment: Qt.AlignTop
                                        visible: delegateItem.isMerge
                                        text: "Merge"
                                        tint: Kirigami.Theme.linkColor
                                    }

                                    RevisionChip {
                                        Layout.alignment: Qt.AlignTop
                                        text: delegateItem.shortSha
                                        commit: delegateItem.sha
                                    }
                                }

                                // Full commit message without the summary
                                Label {
                                    Layout.fillWidth: true
                                    visible: delegateItem.expanded
                                    text: delegateItem.body
                                    textFormat: Text.PlainText
                                    wrapMode: Text.Wrap
                                    color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.8)
                                    bottomPadding: Kirigami.Units.smallSpacing
                                }

                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: Kirigami.Units.smallSpacing

                                    Label {
                                        Layout.maximumWidth: Kirigami.Units.gridUnit * 14
                                        text: delegateItem.author
                                        textFormat: Text.PlainText
                                        font: Kirigami.Theme.smallFont
                                        elide: Text.ElideRight

                                        HoverHandler {
                                            id: authorHover
                                        }

                                        ToolTip.visible: authorHover.hovered && delegateItem.authorEmail !== ""
                                        ToolTip.delay: Kirigami.Units.toolTipDelay
                                        ToolTip.text: delegateItem.author + " <" + delegateItem.authorEmail + ">"
                                    }

                                    Label {
                                        text: "·"
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                    }

                                    Label {
                                        text: delegateItem.timestamp > 0 ? Utils.relativeTime(delegateItem.timestamp, commitsDialog.now) : ""
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor

                                        HoverHandler {
                                            id: dateHover
                                        }

                                        ToolTip.visible: dateHover.hovered && delegateItem.date !== ""
                                        ToolTip.delay: Kirigami.Units.toolTipDelay
                                        ToolTip.text: delegateItem.date
                                    }

                                    Label {
                                        visible: delegateItem.filesChanged >= 0
                                        text: "·"
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                    }

                                    Label {
                                        visible: delegateItem.filesChanged >= 0
                                        text: delegateItem.filesChanged === 1 ? "1 file" : delegateItem.filesChanged + " files"
                                        font: Kirigami.Theme.smallFont
                                        color: Kirigami.Theme.disabledTextColor
                                    }

                                    LineStats {
                                        insertions: delegateItem.insertions
                                        deletions: delegateItem.deletions
                                    }

                                    Item {
                                        Layout.fillWidth: true
                                    }
                                }
                            }

                            RowLayout {
                                Layout.alignment: Qt.AlignTop
                                spacing: 0

                                ToolButton {
                                    icon.name: delegateItem.expanded ? "collapse" : "expand"
                                    display: AbstractButton.IconOnly
                                    text: delegateItem.expanded ? "Hide full commit message" : "Show full commit message"
                                    enabled: delegateItem.hasBody
                                    opacity: delegateItem.hasBody ? 1 : 0
                                    ToolTip.visible: hovered
                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                    ToolTip.text: text
                                    onClicked: delegateItem.toggle()
                                }

                                ToolButton {
                                    icon.name: "globe"
                                    display: AbstractButton.IconOnly
                                    text: "Open on " + commitsDialog.forgeName
                                    enabled: delegateItem.url !== ""
                                    ToolTip.visible: hovered
                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                    ToolTip.text: text
                                    onClicked: Qt.openUrlExternally(delegateItem.url)
                                }
                            }
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: contextMenu.popup()
                        }

                        Menu {
                            id: contextMenu

                            MenuItem {
                                text: "Open on " + commitsDialog.forgeName
                                icon.name: "globe"
                                enabled: delegateItem.url !== ""
                                onTriggered: Qt.openUrlExternally(delegateItem.url)
                            }

                            MenuItem {
                                text: delegateItem.expanded ? "Hide Full Message" : "Show Full Message"
                                icon.name: delegateItem.expanded ? "collapse" : "expand"
                                enabled: delegateItem.hasBody
                                onTriggered: delegateItem.toggle()
                            }

                            MenuSeparator {}

                            MenuItem {
                                text: "Copy Commit Hash"
                                icon.name: "edit-copy"
                                onTriggered: commitsDialog.copyToClipboard(delegateItem.sha)
                            }

                            MenuItem {
                                text: "Copy Link"
                                icon.name: "link"
                                enabled: delegateItem.url !== ""
                                onTriggered: commitsDialog.copyToClipboard(delegateItem.url)
                            }
                        }
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                width: parent.width - (Kirigami.Units.largeSpacing * 4)
                visible: commitListView.count === 0
                icon.name: commitsDialog.filtering ? "search" : commitsDialog.totalCount > 0 ? "view-refresh" : "emblem-success"
                text: commitsDialog.filtering ? "No matching commits" : commitsDialog.totalCount > 0 ? "Commit details aren't loaded yet" : commitsDialog.busy ? "Checking for updates…" : "No new commits"
                explanation: commitsDialog.filtering ? "Try a different search term." : commitsDialog.totalCount > 0 ? "Check for updates to load the new commits." : commitsDialog.busy ? "" : "Your local repository is up to date with the remote repository."
            }
        }
    }

    footer: DialogButtonBox {
        padding: Kirigami.Units.largeSpacing
        topPadding: 0
        spacing: Kirigami.Units.smallSpacing
        alignment: Qt.AlignRight

        Button {
            text: "Pull Without Rebuilding"
            icon.name: "download"
            flat: true
            enabled: !commitsDialog.busy && commitsDialog.totalCount > 0
            DialogButtonBox.buttonRole: DialogButtonBox.ResetRole
            ToolTip.visible: hovered
            ToolTip.delay: Kirigami.Units.toolTipDelay
            ToolTip.text: "Pull the commits without rebuilding, so they are no longer shown as updates.\nThe changes will be applied with the next build."
            onClicked: {
                if (gitManager)
                    gitManager.ignoreUpdate();
            }
        }

        Button {
            text: "Close"
            icon.name: "dialog-close"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }

        Button {
            text: commitsDialog.updateText
            icon.name: "system-software-update"
            highlighted: true
            enabled: commitsDialog.canUpdate
            DialogButtonBox.buttonRole: DialogButtonBox.AcceptRole
            ToolTip.visible: hovered && commitsDialog.updateToolTip !== ""
            ToolTip.delay: Kirigami.Units.toolTipDelay
            ToolTip.text: commitsDialog.updateToolTip
        }
    }
}
