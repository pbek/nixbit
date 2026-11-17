import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import "Utils.js" as Utils

// Lists the packages installed with "nix profile add", updates and removes
// them, and finds new packages in nixpkgs to install
Dialog {
    id: packagesDialog
    title: "Packages"
    parent: Overlay.overlay
    anchors.centerIn: parent
    width: Math.min(Kirigami.Units.gridUnit * 50, parent.width - Kirigami.Units.gridUnit * 2)
    height: Math.min(Kirigami.Units.gridUnit * 42, parent.height - Kirigami.Units.gridUnit * 2)
    modal: true
    padding: Kirigami.Units.largeSpacing

    readonly property int installedTab: 0
    readonly property int searchTab: 1
    property int currentTab: installedTab

    readonly property var packages: packageManager ? packageManager.packages : []
    readonly property int packageCount: packageManager ? packageManager.packageCount : 0
    readonly property int updateCount: packageManager ? packageManager.updateCount : 0
    readonly property bool loading: packageManager ? packageManager.isLoading : false
    readonly property bool checkingUpdates: packageManager ? packageManager.isCheckingUpdates : false
    readonly property bool busy: packageManager ? packageManager.isBusy : false
    readonly property string operation: packageManager ? packageManager.operation : ""
    readonly property var operationTargets: packageManager ? packageManager.operationTargets : []
    readonly property double lastUpdateCheck: packageManager ? packageManager.lastUpdateCheck : 0
    readonly property bool filtering: filterField.text.trim() !== ""

    // Current time for relative dates, refreshed while the dialog is open
    property double now: Date.now()
    // Names of the packages whose details are shown
    property var expandedNames: ({})
    property bool outputVisible: false

    // Result of the last install, update or removal
    property string resultMessage: ""
    property bool resultSuccess: true
    // How the operation can be retried, see PackageManager::operationFinished
    property string resultHint: ""
    property string resultOperation: ""

    function openInstalled() {
        currentTab = installedTab;
        open();
    }

    function openSearch() {
        currentTab = searchTab;
        open();
    }

    function isOperationTarget(name) {
        if (!busy)
            return false;
        // Updating all packages
        if (operation === "upgrade" && operationTargets.length === 0)
            return true;
        return operationTargets.indexOf(name) !== -1;
    }

    function operationTitle() {
        var target = operationTargets.length === 1 ? operationTargets[0] : operationTargets.length > 1 ? operationTargets.length + " packages" : "all packages";
        if (operation === "install")
            return "Installing " + target;
        if (operation === "remove")
            return "Removing " + target;
        return "Updating " + target;
    }

    function confirmRemove(names) {
        removeDialog.names = names;
        removeDialog.open();
    }

    function copyToClipboard(text) {
        clipboardHelper.text = text;
        clipboardHelper.selectAll();
        clipboardHelper.copy();
        clipboardHelper.text = "";
    }

    function toggleExpanded(name) {
        var expanded = Object.assign({}, expandedNames);
        if (expanded[name])
            delete expanded[name];
        else
            expanded[name] = true;
        expandedNames = expanded;
    }

    function searchUrl(attrName) {
        return "https://search.nixos.org/packages?channel=unstable&show=" + encodeURIComponent(attrName) + "&query=" + encodeURIComponent(attrName);
    }

    // Packages shown in the installed list, filtered and sorted
    function rebuildInstalledModel() {
        var query = filterField.text.trim().toLowerCase();
        var items = [];
        for (var i = 0; i < packages.length; i++) {
            var pkg = packages[i];
            if (query !== "") {
                var haystack = [pkg.name, pkg.attrName, pkg.description, pkg.mainProgram, pkg.pname].join("\n").toLowerCase();
                if (haystack.indexOf(query) === -1)
                    continue;
            }
            items.push(pkg);
        }

        var sortMode = sortComboBox.currentIndex;
        items.sort(function (a, b) {
            // Packages with updates first
            var aUpdate = a.updateState === "available" ? 0 : 1;
            var bUpdate = b.updateState === "available" ? 0 : 1;
            if (aUpdate !== bUpdate)
                return aUpdate - bUpdate;
            if (sortMode === 1 && a.closureSize !== b.closureSize)
                return b.closureSize - a.closureSize;
            if (sortMode === 2) {
                var aChanged = Math.max(a.installedAt, a.updatedAt);
                var bChanged = Math.max(b.installedAt, b.updatedAt);
                if (aChanged !== bChanged)
                    return bChanged - aChanged;
            }
            return a.name.toLowerCase() < b.name.toLowerCase() ? -1 : a.name.toLowerCase() > b.name.toLowerCase() ? 1 : 0;
        });

        // Update the rows in place if the order didn't change, so expanded
        // rows and the scroll position are kept while details are loaded
        var sameOrder = installedModel.count === items.length;
        for (var j = 0; sameOrder && j < items.length; j++) {
            if (installedModel.get(j).name !== items[j].name)
                sameOrder = false;
        }

        if (!sameOrder)
            installedModel.clear();
        for (var k = 0; k < items.length; k++) {
            var row = toRow(items[k]);
            if (sameOrder)
                installedModel.set(k, row);
            else
                installedModel.append(row);
        }
    }

    // ListModel can't hold arrays as plain values, so they are joined
    function toRow(pkg) {
        return {
            name: pkg.name,
            attrName: pkg.attrName,
            attrPath: pkg.attrPath,
            originalUrl: pkg.originalUrl,
            source: pkg.source,
            revision: pkg.revision,
            revisionUrl: pkg.revisionUrl,
            installCommand: pkg.installCommand,
            storePathsText: pkg.storePaths.join("\n"),
            outputsText: pkg.outputs.join(", "),
            active: pkg.active,
            priority: pkg.priority,
            metadataLoaded: pkg.metadataLoaded,
            missing: pkg.missing,
            pname: pkg.pname,
            version: pkg.version,
            description: pkg.description,
            longDescription: pkg.longDescription,
            homepage: pkg.homepage,
            mainProgram: pkg.mainProgram,
            licensesText: pkg.licenses.join(", "),
            unfree: pkg.unfree,
            insecure: pkg.insecure,
            broken: pkg.broken,
            closureSize: pkg.closureSize,
            installedAt: pkg.installedAt,
            installedBeforeHistory: pkg.installedBeforeHistory,
            updatedAt: pkg.updatedAt,
            updateState: pkg.updateState,
            newVersion: pkg.newVersion,
            isRebuild: pkg.isRebuild,
            latestRevision: pkg.latestRevision,
            latestRevisionUrl: pkg.latestRevisionUrl
        };
    }

    // Short summary of when the package was installed or updated
    function changeText(installedAt, installedBeforeHistory, updatedAt) {
        if (updatedAt > 0)
            return "Updated " + Utils.relativeTime(updatedAt, now);
        if (installedAt > 0)
            return (installedBeforeHistory ? "Installed over " : "Installed ") + Utils.relativeTime(installedAt, now);
        return "";
    }

    function formatDate(timestamp) {
        return timestamp > 0 ? new Date(timestamp).toLocaleString(Qt.locale(), Locale.ShortFormat) : "";
    }

    onAboutToShow: {
        now = Date.now();
        filterField.clear();
        if (packageManager) {
            packageManager.refresh();
            packageManager.checkForUpdatesIfStale(60);
        }
        rebuildInstalledModel();
        if (currentTab === searchTab)
            Qt.callLater(searchField.forceActiveFocus);
    }

    onPackagesChanged: rebuildInstalledModel()

    onCurrentTabChanged: {
        if (currentTab === searchTab && visible)
            Qt.callLater(searchField.forceActiveFocus);
    }

    Timer {
        interval: 60000
        repeat: true
        running: packagesDialog.visible
        onTriggered: packagesDialog.now = Date.now()
    }

    Connections {
        target: packageManager

        function onOperationFinished(success, operation, targets, message, hint) {
            packagesDialog.resultSuccess = success;
            packagesDialog.resultMessage = message;
            packagesDialog.resultHint = hint;
            packagesDialog.resultOperation = operation;
            resultMessageBox.visible = true;
            // The output explains why it failed
            if (success)
                packagesDialog.outputVisible = false;
        }

        function onOperationChanged() {
            if (packageManager.isBusy)
                resultMessageBox.visible = false;
        }
    }

    ListModel {
        id: installedModel
    }

    // Used to put text on the clipboard
    TextEdit {
        id: clipboardHelper
        visible: false
    }

    Shortcut {
        sequences: [StandardKey.Find]
        enabled: packagesDialog.visible
        onActivated: {
            if (packagesDialog.currentTab === packagesDialog.searchTab)
                searchField.forceActiveFocus();
            else
                filterField.forceActiveFocus();
        }
    }

    Kirigami.PromptDialog {
        id: removeDialog
        property var names: []
        parent: Overlay.overlay
        title: names.length === 1 ? "Remove " + names[0] + "?" : "Remove " + names.length + " packages?"
        subtitle: "The package will be removed from your profile with nix profile remove. You can switch back to the previous profile generation with nix profile rollback."
        dialogType: Kirigami.PromptDialog.Warning
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: "Remove"
                icon.name: "edit-delete-remove"
                onTriggered: removeDialog.accept()
            }
        ]
        onAccepted: packageManager.remove(names)
    }

    // Small rounded label, e.g. "Unfree"
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

    // Version in a subtle monospace chip
    component VersionChip: Label {
        visible: text !== ""
        font.family: Kirigami.Theme.fixedWidthFont.family
        font.pointSize: Kirigami.Theme.smallFont.pointSize
        color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.75)
        leftPadding: Kirigami.Units.smallSpacing
        rightPadding: Kirigami.Units.smallSpacing
        topPadding: 1
        bottomPadding: 1
        elide: Text.ElideRight
        background: Rectangle {
            radius: Kirigami.Units.cornerRadius
            color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.07)
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.18)
        }
    }

    // Icon of the package from the icon theme, or a tile with its initial
    component PackageIcon: Item {
        id: packageIcon
        property string name
        property var iconNames: []

        readonly property string initial: {
            var last = name.split(".").pop().replace(/^[^A-Za-z0-9]+/, "");
            return last.length > 0 ? last.charAt(0).toUpperCase() : "?";
        }
        readonly property real hue: {
            var hash = 0;
            for (var i = 0; i < name.length; i++)
                hash = (hash * 31 + name.charCodeAt(i)) % 360;
            return hash / 360;
        }
        readonly property bool dark: Kirigami.ColorUtils.brightnessForColor(Kirigami.Theme.backgroundColor) === Kirigami.ColorUtils.Dark

        implicitWidth: Kirigami.Units.iconSizes.medium + Kirigami.Units.smallSpacing
        implicitHeight: implicitWidth
        Accessible.ignored: true

        // Applications usually have an icon named after the program or package
        Kirigami.Icon {
            id: themeIcon
            anchors.fill: parent
            source: packageManager ? packageManager.themeIconName(packageIcon.iconNames) : ""
            visible: source !== ""
        }

        Rectangle {
            anchors.fill: parent
            visible: !themeIcon.visible
            radius: Kirigami.Units.cornerRadius * 2
            gradient: Gradient {
                GradientStop {
                    position: 0
                    color: Qt.hsla(packageIcon.hue, 0.5, packageIcon.dark ? 0.44 : 0.6, 1)
                }
                GradientStop {
                    position: 1
                    color: Qt.hsla(packageIcon.hue, 0.55, packageIcon.dark ? 0.34 : 0.48, 1)
                }
            }

            Label {
                anchors.centerIn: parent
                text: packageIcon.initial
                color: "white"
                font.pixelSize: parent.height * 0.48
                font.weight: Font.DemiBold
            }
        }
    }

    // Caption and value in the details of a package
    component DetailRow: RowLayout {
        property alias caption: captionLabel.text
        default property alias content: valueContainer.data

        Layout.fillWidth: true
        spacing: Kirigami.Units.largeSpacing

        Label {
            id: captionLabel
            Layout.alignment: Qt.AlignTop
            Layout.preferredWidth: Kirigami.Units.gridUnit * 6
            horizontalAlignment: Text.AlignRight
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
        }

        ColumnLayout {
            id: valueContainer
            Layout.fillWidth: true
            spacing: 0
        }
    }

    // Label with a monospace text that can be copied
    component CopyableText: RowLayout {
        property alias text: valueText.text
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing

        TextEdit {
            id: valueText
            Layout.fillWidth: true
            readOnly: true
            selectByMouse: true
            wrapMode: TextEdit.WrapAnywhere
            font.family: Kirigami.Theme.fixedWidthFont.family
            font.pointSize: Kirigami.Theme.smallFont.pointSize
            color: Kirigami.Theme.textColor
            selectionColor: Kirigami.Theme.highlightColor
            selectedTextColor: Kirigami.Theme.highlightedTextColor
        }

        ToolButton {
            Layout.alignment: Qt.AlignTop
            implicitWidth: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing * 2
            implicitHeight: implicitWidth
            padding: Kirigami.Units.smallSpacing / 2
            icon.name: "edit-copy"
            icon.width: Kirigami.Units.iconSizes.small
            icon.height: Kirigami.Units.iconSizes.small
            display: AbstractButton.IconOnly
            text: "Copy"
            ToolTip.visible: hovered
            ToolTip.delay: Kirigami.Units.toolTipDelay
            ToolTip.text: text
            onClicked: packagesDialog.copyToClipboard(valueText.text)
        }
    }

    // Small text separated by dots, e.g. "nixpkgs · 38.5 MiB · MIT"
    component MetaSeparator: Label {
        text: "·"
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
    }

    component MetaLabel: Label {
        font: Kirigami.Theme.smallFont
        color: Kirigami.Theme.disabledTextColor
        textFormat: Text.PlainText
        elide: Text.ElideRight
    }

    // Frame for the package lists
    component ListFrame: Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Kirigami.Theme.colorSet: Kirigami.Theme.View
        Kirigami.Theme.inherit: false
        color: Kirigami.Theme.backgroundColor
        radius: Kirigami.Units.cornerRadius
        border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.2)
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        TabBar {
            id: tabBar
            Layout.fillWidth: true
            currentIndex: packagesDialog.currentTab
            onCurrentIndexChanged: packagesDialog.currentTab = currentIndex

            TabButton {
                text: packagesDialog.updateCount > 0 ? "Installed (" + packagesDialog.updateCount + (packagesDialog.updateCount === 1 ? " update)" : " updates)") : packagesDialog.packageCount > 0 ? "Installed (" + packagesDialog.packageCount + ")" : "Installed"
                icon.name: packagesDialog.updateCount > 0 ? "system-software-update" : "package-x-generic"
            }

            TabButton {
                text: "Find Packages"
                icon.name: "search"
            }
        }

        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            // The hidden tab has no width, so its wrapped texts would make it
            // much taller than the dialog
            Layout.minimumHeight: 0
            currentIndex: packagesDialog.currentTab

            // Installed packages
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                Rectangle {
                    id: summaryCard
                    readonly property bool hasError: packageManager !== null && packageManager.error !== ""
                    readonly property color tint: hasError ? Kirigami.Theme.negativeTextColor : packagesDialog.updateCount > 0 ? Kirigami.Theme.neutralTextColor : packagesDialog.lastUpdateCheck > 0 && packagesDialog.packageCount > 0 ? Kirigami.Theme.positiveTextColor : Kirigami.Theme.highlightColor

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
                                source: summaryCard.hasError ? "dialog-error" : packagesDialog.updateCount > 0 ? "system-software-update" : packagesDialog.lastUpdateCheck > 0 && packagesDialog.packageCount > 0 ? "emblem-success" : "package-x-generic"
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                Layout.fillWidth: true
                                level: 2
                                elide: Text.ElideRight
                                text: {
                                    if (summaryCard.hasError)
                                        return "Packages couldn't be loaded";
                                    if (!packageManager || !packageManager.loaded)
                                        return "Loading your packages…";
                                    if (packagesDialog.packageCount === 0)
                                        return "No packages installed yet";
                                    if (packagesDialog.updateCount > 0)
                                        return packagesDialog.updateCount === 1 ? "1 update available" : packagesDialog.updateCount + " updates available";
                                    if (packagesDialog.checkingUpdates)
                                        return "Checking for updates…";
                                    if (packagesDialog.lastUpdateCheck > 0)
                                        return packagesDialog.packageCount === 1 ? "Your package is up to date" : "All " + packagesDialog.packageCount + " packages are up to date";
                                    return packagesDialog.packageCount === 1 ? "1 package installed" : packagesDialog.packageCount + " packages installed";
                                }
                            }

                            Label {
                                Layout.fillWidth: true
                                textFormat: Text.PlainText
                                wrapMode: Text.Wrap
                                color: Kirigami.Theme.disabledTextColor
                                text: {
                                    if (summaryCard.hasError)
                                        return packageManager.error;
                                    if (packagesDialog.checkingUpdates && packageManager.updateCheckStatus !== "")
                                        return packageManager.updateCheckStatus;
                                    if (packagesDialog.packageCount === 0)
                                        return "Packages you install for your user with nix profile add are listed here. They don't change your system configuration.";
                                    var checked = packagesDialog.lastUpdateCheck > 0 ? " Checked for updates " + Utils.relativeTime(packagesDialog.lastUpdateCheck, packagesDialog.now) + "." : " Not checked for updates yet.";
                                    return "Installed for your user with nix profile add, separate from your system configuration." + checked;
                                }
                            }

                            Flow {
                                Layout.fillWidth: true
                                Layout.topMargin: Kirigami.Units.smallSpacing
                                visible: packagesDialog.packageCount > 0 && !summaryCard.hasError
                                spacing: Kirigami.Units.largeSpacing

                                Row {
                                    spacing: Kirigami.Units.smallSpacing

                                    Kirigami.Icon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Kirigami.Units.iconSizes.small
                                        height: width
                                        source: "user-home"
                                        color: Kirigami.Theme.disabledTextColor
                                    }

                                    Label {
                                        text: packageManager ? packageManager.profilePath : ""
                                        font.family: Kirigami.Theme.fixedWidthFont.family
                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                    }
                                }

                                Row {
                                    spacing: Kirigami.Units.smallSpacing
                                    visible: packageManager !== null && packageManager.totalSize > 0

                                    HoverHandler {
                                        id: totalSizeHover
                                    }

                                    ToolTip.visible: totalSizeHover.hovered
                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                    ToolTip.text: "Disk space of all packages including their dependencies. Dependencies shared with your system are counted as well."

                                    Kirigami.Icon {
                                        anchors.verticalCenter: parent.verticalCenter
                                        width: Kirigami.Units.iconSizes.small
                                        height: width
                                        source: "drive-harddisk"
                                        color: Kirigami.Theme.disabledTextColor
                                    }

                                    Label {
                                        text: packageManager ? Utils.formatBytes(packageManager.totalSize) + " on disk" : ""
                                        font: Kirigami.Theme.smallFont
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
                                    running: packagesDialog.checkingUpdates || packagesDialog.loading
                                    visible: running
                                }

                                ToolButton {
                                    icon.name: "view-refresh"
                                    display: AbstractButton.IconOnly
                                    text: "Check for updates"
                                    visible: !packagesDialog.checkingUpdates && !packagesDialog.loading
                                    enabled: packagesDialog.packageCount > 0 && !packagesDialog.busy
                                    ToolTip.visible: hovered
                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                    ToolTip.text: "Check for updates by comparing your packages with the newest version of the flake they were installed from"
                                    onClicked: packageManager.checkForUpdates()
                                }
                            }

                            Button {
                                Layout.alignment: Qt.AlignRight
                                visible: packagesDialog.packageCount > 0
                                text: packagesDialog.updateCount > 1 ? "Update All (" + packagesDialog.updateCount + ")" : "Update All"
                                icon.name: "system-software-update"
                                highlighted: packagesDialog.updateCount > 0
                                flat: packagesDialog.updateCount === 0
                                enabled: !packagesDialog.busy
                                ToolTip.visible: hovered
                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                ToolTip.text: "Update all packages to the newest version of the flake they were installed from (nix profile upgrade --all)"
                                onClicked: packageManager.upgradeAll()
                            }
                        }
                    }
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: packageManager !== null && packageManager.updateCheckError !== "" && !packagesDialog.checkingUpdates
                    type: Kirigami.MessageType.Warning
                    text: packageManager ? "Some updates couldn't be checked. " + packageManager.updateCheckError : ""
                    actions: [
                        Kirigami.Action {
                            text: "Try Again"
                            icon.name: "view-refresh"
                            onTriggered: packageManager.checkForUpdates()
                        }
                    ]
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: packagesDialog.packageCount > 1
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.SearchField {
                        id: filterField
                        Layout.fillWidth: true
                        placeholderText: "Filter installed packages…"
                        onTextChanged: packagesDialog.rebuildInstalledModel()
                    }

                    ComboBox {
                        id: sortComboBox
                        model: ["Sort by Name", "Sort by Size", "Recently Changed"]
                        onCurrentIndexChanged: packagesDialog.rebuildInstalledModel()
                    }
                }

                ListFrame {
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true

                        ListView {
                            id: installedListView
                            model: installedModel
                            activeFocusOnTab: true
                            keyNavigationEnabled: true
                            currentIndex: -1
                            boundsBehavior: Flickable.StopAtBounds

                            delegate: ItemDelegate {
                                id: installedDelegate
                                required property int index
                                required property var model

                                readonly property bool expanded: packagesDialog.expandedNames[model.name] === true
                                readonly property bool working: packagesDialog.isOperationTarget(model.name)
                                readonly property bool hasUpdate: model.updateState === "available"
                                readonly property string changed: packagesDialog.changeText(model.installedAt, model.installedBeforeHistory, model.updatedAt)

                                width: ListView.view.width
                                padding: Kirigami.Units.largeSpacing
                                topPadding: Kirigami.Units.mediumSpacing
                                bottomPadding: Kirigami.Units.mediumSpacing
                                hoverEnabled: true
                                down: false
                                highlighted: ListView.isCurrentItem && installedListView.activeFocus
                                Accessible.name: model.name + " " + model.version + (hasUpdate ? ", update available" : "")

                                onClicked: packagesDialog.toggleExpanded(model.name)
                                Keys.onSpacePressed: packagesDialog.toggleExpanded(model.name)
                                Keys.onReturnPressed: packagesDialog.toggleExpanded(model.name)

                                background: Rectangle {
                                    color: {
                                        if (installedDelegate.working)
                                            return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.1);
                                        if (installedDelegate.highlighted)
                                            return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.18);
                                        if (installedDelegate.hovered)
                                            return Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.08);
                                        return "transparent";
                                    }

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: Kirigami.Units.shortDuration
                                        }
                                    }

                                    // Accent for packages with updates
                                    Rectangle {
                                        anchors.left: parent.left
                                        anchors.top: parent.top
                                        anchors.bottom: parent.bottom
                                        width: 3
                                        visible: installedDelegate.hasUpdate
                                        color: Kirigami.Theme.neutralTextColor
                                    }

                                    Kirigami.Separator {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        visible: installedDelegate.index < installedListView.count - 1
                                        opacity: 0.6
                                    }
                                }

                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.largeSpacing

                                    PackageIcon {
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 2
                                        name: installedDelegate.model.name
                                        iconNames: [installedDelegate.model.mainProgram, installedDelegate.model.pname, installedDelegate.model.name]
                                        opacity: installedDelegate.model.active ? 1 : 0.5
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Label {
                                                Layout.minimumWidth: Math.min(implicitWidth, Kirigami.Units.gridUnit * 4)
                                                text: installedDelegate.model.name
                                                textFormat: Text.PlainText
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }

                                            VersionChip {
                                                Layout.maximumWidth: Kirigami.Units.gridUnit * 8
                                                text: installedDelegate.model.version
                                            }

                                            // Version the package can be updated to
                                            Row {
                                                visible: installedDelegate.hasUpdate
                                                spacing: Kirigami.Units.smallSpacing

                                                Kirigami.Icon {
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    width: Kirigami.Units.iconSizes.small
                                                    height: width
                                                    source: "go-next"
                                                    color: Kirigami.Theme.neutralTextColor
                                                }

                                                Badge {
                                                    text: installedDelegate.model.isRebuild ? "Rebuilt" : installedDelegate.model.newVersion !== "" ? installedDelegate.model.newVersion : "Newer build"
                                                    tint: Kirigami.Theme.neutralTextColor
                                                    font.family: installedDelegate.model.isRebuild ? Kirigami.Theme.smallFont.family : Kirigami.Theme.fixedWidthFont.family

                                                    HoverHandler {
                                                        id: newVersionHover
                                                    }

                                                    ToolTip.visible: newVersionHover.hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    ToolTip.text: installedDelegate.model.isRebuild ? "The version stays the same, but the package was rebuilt, e.g. because a dependency was updated." : "Version " + installedDelegate.model.newVersion + " is available."
                                                }
                                            }

                                            Badge {
                                                visible: installedDelegate.model.unfree
                                                text: "Unfree"
                                                tint: Kirigami.Theme.disabledTextColor
                                            }

                                            Badge {
                                                visible: installedDelegate.model.insecure
                                                text: "Insecure"
                                                tint: Kirigami.Theme.negativeTextColor
                                            }

                                            Badge {
                                                visible: installedDelegate.model.broken
                                                text: "Broken"
                                                tint: Kirigami.Theme.negativeTextColor
                                            }

                                            Badge {
                                                visible: !installedDelegate.model.active
                                                text: "Inactive"
                                                tint: Kirigami.Theme.disabledTextColor
                                            }

                                            Badge {
                                                visible: installedDelegate.model.missing
                                                text: "Not found in " + installedDelegate.model.source
                                                tint: Kirigami.Theme.neutralTextColor
                                            }

                                            Item {
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            text: installedDelegate.model.description !== "" ? installedDelegate.model.description : installedDelegate.model.metadataLoaded ? "No description" : "Loading details…"
                                            textFormat: Text.PlainText
                                            wrapMode: Text.Wrap
                                            maximumLineCount: installedDelegate.expanded ? 6 : 2
                                            elide: Text.ElideRight
                                            color: installedDelegate.model.description !== "" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                                            font.italic: installedDelegate.model.description === ""
                                        }

                                        // Progress of the running operation
                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: installedDelegate.working
                                            spacing: Kirigami.Units.smallSpacing

                                            BusyIndicator {
                                                implicitWidth: Kirigami.Units.iconSizes.small
                                                implicitHeight: implicitWidth
                                                padding: 0
                                                running: installedDelegate.working
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: packageManager ? packageManager.operationStatus : ""
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.highlightColor
                                                elide: Text.ElideRight
                                            }
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: !installedDelegate.working
                                            spacing: Kirigami.Units.smallSpacing

                                            MetaLabel {
                                                text: installedDelegate.model.source
                                            }

                                            MetaSeparator {
                                                visible: installedDelegate.model.closureSize >= 0
                                            }

                                            MetaLabel {
                                                visible: installedDelegate.model.closureSize >= 0
                                                text: Utils.formatBytes(installedDelegate.model.closureSize)

                                                HoverHandler {
                                                    id: sizeHover
                                                }

                                                ToolTip.visible: sizeHover.hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                ToolTip.text: "Disk space of the package including its dependencies. Dependencies shared with other packages only take space once."
                                            }

                                            MetaSeparator {
                                                visible: installedDelegate.model.licensesText !== ""
                                            }

                                            MetaLabel {
                                                Layout.maximumWidth: Kirigami.Units.gridUnit * 10
                                                visible: installedDelegate.model.licensesText !== ""
                                                text: installedDelegate.model.licensesText
                                            }

                                            MetaSeparator {
                                                visible: installedDelegate.changed !== ""
                                            }

                                            MetaLabel {
                                                visible: installedDelegate.changed !== ""
                                                text: installedDelegate.changed

                                                HoverHandler {
                                                    id: changedHover
                                                }

                                                ToolTip.visible: changedHover.hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                ToolTip.text: (installedDelegate.model.installedAt > 0 ? "Installed: " + packagesDialog.formatDate(installedDelegate.model.installedAt) + (installedDelegate.model.installedBeforeHistory ? " or earlier" : "") : "") + (installedDelegate.model.updatedAt > 0 ? "\nUpdated: " + packagesDialog.formatDate(installedDelegate.model.updatedAt) : "")
                                            }

                                            Item {
                                                Layout.fillWidth: true
                                            }
                                        }

                                        // Details
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            Layout.topMargin: Kirigami.Units.smallSpacing
                                            visible: installedDelegate.expanded
                                            spacing: Kirigami.Units.smallSpacing

                                            Label {
                                                Layout.fillWidth: true
                                                Layout.bottomMargin: Kirigami.Units.smallSpacing
                                                visible: text !== ""
                                                text: installedDelegate.model.longDescription
                                                textFormat: Text.PlainText
                                                wrapMode: Text.Wrap
                                                color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.textColor, 0.8)
                                            }

                                            DetailRow {
                                                caption: "Homepage"
                                                visible: installedDelegate.model.homepage !== ""

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "<a href=\"" + installedDelegate.model.homepage + "\">" + installedDelegate.model.homepage.replace(/&/g, "&amp;").replace(/</g, "&lt;") + "</a>"
                                                    textFormat: Text.StyledText
                                                    font: Kirigami.Theme.smallFont
                                                    elide: Text.ElideRight
                                                    onLinkActivated: link => Qt.openUrlExternally(link)

                                                    HoverHandler {
                                                        cursorShape: Qt.PointingHandCursor
                                                    }
                                                }
                                            }

                                            DetailRow {
                                                caption: "Command"
                                                visible: installedDelegate.model.mainProgram !== ""

                                                CopyableText {
                                                    text: installedDelegate.model.mainProgram
                                                }
                                            }

                                            DetailRow {
                                                caption: "Installed from"

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: Kirigami.Units.smallSpacing

                                                    Label {
                                                        text: installedDelegate.model.originalUrl + "#" + installedDelegate.model.attrName + (installedDelegate.model.outputsText !== "" ? " (" + installedDelegate.model.outputsText + ")" : "")
                                                        textFormat: Text.PlainText
                                                        font.family: Kirigami.Theme.fixedWidthFont.family
                                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                                        elide: Text.ElideMiddle
                                                        Layout.fillWidth: true
                                                        Layout.maximumWidth: implicitWidth
                                                    }

                                                    Label {
                                                        visible: installedDelegate.model.revision !== ""
                                                        text: installedDelegate.model.revisionUrl !== "" ? "<a href=\"" + installedDelegate.model.revisionUrl + "\">" + installedDelegate.model.revision + "</a>" : installedDelegate.model.revision
                                                        textFormat: installedDelegate.model.revisionUrl !== "" ? Text.StyledText : Text.PlainText
                                                        font.family: Kirigami.Theme.fixedWidthFont.family
                                                        font.pointSize: Kirigami.Theme.smallFont.pointSize
                                                        onLinkActivated: link => Qt.openUrlExternally(link)
                                                    }
                                                }
                                            }

                                            DetailRow {
                                                caption: "Update"
                                                visible: installedDelegate.hasUpdate && installedDelegate.model.latestRevision !== ""

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: "Available from " + installedDelegate.model.source + " " + (installedDelegate.model.latestRevisionUrl !== "" ? "<a href=\"" + installedDelegate.model.latestRevisionUrl + "\">" + installedDelegate.model.latestRevision + "</a>" : installedDelegate.model.latestRevision)
                                                    textFormat: Text.StyledText
                                                    font: Kirigami.Theme.smallFont
                                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                                }
                                            }

                                            DetailRow {
                                                caption: "Installed"
                                                visible: installedDelegate.model.installedAt > 0

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: packagesDialog.formatDate(installedDelegate.model.installedAt) + (installedDelegate.model.installedBeforeHistory ? " or earlier" : "") + (installedDelegate.model.updatedAt > 0 ? ", updated " + packagesDialog.formatDate(installedDelegate.model.updatedAt) : "")
                                                    font: Kirigami.Theme.smallFont
                                                    wrapMode: Text.Wrap
                                                }
                                            }

                                            DetailRow {
                                                caption: "Priority"
                                                visible: installedDelegate.model.priority !== 5

                                                Label {
                                                    Layout.fillWidth: true
                                                    text: installedDelegate.model.priority + (installedDelegate.model.priority < 5 ? " (wins conflicts with other packages)" : " (loses conflicts with other packages)")
                                                    font: Kirigami.Theme.smallFont
                                                }
                                            }

                                            DetailRow {
                                                caption: "Store paths"

                                                CopyableText {
                                                    text: installedDelegate.model.storePathsText
                                                }
                                            }
                                        }
                                    }

                                    RowLayout {
                                        Layout.alignment: Qt.AlignTop
                                        spacing: 0

                                        Button {
                                            visible: installedDelegate.hasUpdate && !installedDelegate.working
                                            text: "Update"
                                            icon.name: "system-software-update"
                                            enabled: !packagesDialog.busy
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Kirigami.Units.toolTipDelay
                                            ToolTip.text: installedDelegate.model.isRebuild || installedDelegate.model.newVersion === "" ? "Install the newer build of " + installedDelegate.model.name : "Update " + installedDelegate.model.name + " from " + installedDelegate.model.version + " to " + installedDelegate.model.newVersion
                                            onClicked: packageManager.upgrade([installedDelegate.model.name])
                                        }

                                        ToolButton {
                                            icon.name: installedDelegate.expanded ? "collapse" : "expand"
                                            display: AbstractButton.IconOnly
                                            text: installedDelegate.expanded ? "Hide details" : "Show details"
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Kirigami.Units.toolTipDelay
                                            ToolTip.text: text
                                            onClicked: packagesDialog.toggleExpanded(installedDelegate.model.name)
                                        }

                                        ToolButton {
                                            icon.name: "overflow-menu"
                                            display: AbstractButton.IconOnly
                                            text: "More actions"
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Kirigami.Units.toolTipDelay
                                            ToolTip.text: text
                                            onClicked: installedMenu.popup()
                                        }
                                    }
                                }

                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: installedMenu.popup()
                                }

                                Menu {
                                    id: installedMenu

                                    MenuItem {
                                        text: "Update"
                                        icon.name: "system-software-update"
                                        visible: installedDelegate.hasUpdate
                                        height: visible ? implicitHeight : 0
                                        enabled: !packagesDialog.busy
                                        onTriggered: packageManager.upgrade([installedDelegate.model.name])
                                    }

                                    MenuItem {
                                        text: "Open Homepage"
                                        icon.name: "globe"
                                        enabled: installedDelegate.model.homepage !== ""
                                        onTriggered: Qt.openUrlExternally(installedDelegate.model.homepage)
                                    }

                                    MenuItem {
                                        text: "Show on search.nixos.org"
                                        icon.name: "search"
                                        enabled: installedDelegate.model.source.indexOf("nixpkgs") === 0
                                        onTriggered: Qt.openUrlExternally(packagesDialog.searchUrl(installedDelegate.model.attrName))
                                    }

                                    MenuSeparator {}

                                    MenuItem {
                                        text: "Copy Command Name"
                                        icon.name: "edit-copy"
                                        enabled: installedDelegate.model.mainProgram !== ""
                                        onTriggered: packagesDialog.copyToClipboard(installedDelegate.model.mainProgram)
                                    }

                                    MenuItem {
                                        text: "Copy Install Command"
                                        icon.name: "edit-copy"
                                        onTriggered: packagesDialog.copyToClipboard(installedDelegate.model.installCommand)
                                    }

                                    MenuItem {
                                        text: "Copy Store Path"
                                        icon.name: "edit-copy"
                                        onTriggered: packagesDialog.copyToClipboard(installedDelegate.model.storePathsText.split("\n")[0])
                                    }

                                    MenuSeparator {}

                                    MenuItem {
                                        text: "Remove…"
                                        icon.name: "edit-delete-remove"
                                        enabled: !packagesDialog.busy
                                        onTriggered: packagesDialog.confirmRemove([installedDelegate.model.name])
                                    }
                                }
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - (Kirigami.Units.largeSpacing * 4)
                        visible: installedListView.count === 0
                        icon.name: packagesDialog.filtering ? "search" : packageManager && packageManager.loaded ? "package-x-generic" : "view-refresh"
                        text: {
                            if (packagesDialog.filtering)
                                return "No matching packages";
                            if (!packageManager || !packageManager.loaded)
                                return packageManager && packageManager.error !== "" ? "Packages couldn't be loaded" : "Loading packages…";
                            return "Find your first package";
                        }
                        explanation: {
                            if (packagesDialog.filtering)
                                return "Try a different search term.";
                            if (!packageManager || !packageManager.loaded)
                                return "";
                            return "Find packages in nixpkgs and install them for your user, without rebuilding your system.";
                        }
                        helpfulAction: Kirigami.Action {
                            enabled: !packagesDialog.filtering && packageManager !== null && packageManager.loaded
                            text: "Find Packages"
                            icon.name: "search"
                            onTriggered: packagesDialog.currentTab = packagesDialog.searchTab
                        }
                    }
                }

                Label {
                    Layout.fillWidth: true
                    visible: packageManager !== null && packageManager.otherEntries.length > 0
                    text: {
                        if (!packageManager)
                            return "";
                        var entries = packageManager.otherEntries;
                        return (entries.length === 1 ? "1 other profile entry isn't" : entries.length + " other profile entries aren't") + " installed from a flake and can't be managed here: " + entries.join(", ");
                    }
                    textFormat: Text.PlainText
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                }
            }

            // Find packages in nixpkgs
            ColumnLayout {
                spacing: Kirigami.Units.largeSpacing

                readonly property bool active: packagesDialog.visible && packagesDialog.currentTab === packagesDialog.searchTab
                onActiveChanged: {
                    if (active && packageSearch)
                        packageSearch.loadIndex();
                }

                Kirigami.SearchField {
                    id: searchField
                    Layout.fillWidth: true
                    placeholderText: "Search nixpkgs by name or description…"
                    // Results are calculated locally, so they can update while typing
                    delaySearch: false
                    onTextChanged: searchDebounce.restart()
                    Keys.onDownPressed: {
                        if (searchListView.count > 0) {
                            searchListView.currentIndex = 0;
                            searchListView.forceActiveFocus();
                        }
                    }

                    Timer {
                        id: searchDebounce
                        interval: 120
                        onTriggered: packageSearch.search(searchField.text)
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: packageSearch !== null && packageSearch.indexReady
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        implicitWidth: Kirigami.Units.iconSizes.small
                        implicitHeight: implicitWidth
                        source: "view-list-details"
                        color: Kirigami.Theme.disabledTextColor
                    }

                    MetaLabel {
                        Layout.fillWidth: true
                        text: {
                            if (!packageSearch)
                                return "";
                            var source = "nixpkgs" + (packageSearch.nixpkgsVersion !== "" ? " " + packageSearch.nixpkgsVersion : "");
                            if (packageSearch.query.trim() === "")
                                return Utils.formatNumber(packageSearch.indexSize) + " packages available in " + source;
                            if (packageSearch.matchCount > packageSearch.results.length)
                                return "Showing the best " + packageSearch.results.length + " of " + Utils.formatNumber(packageSearch.matchCount) + " matches in " + source;
                            return (packageSearch.matchCount === 1 ? "1 match" : Utils.formatNumber(packageSearch.matchCount) + " matches") + " in " + source;
                        }
                    }

                    ToolButton {
                        icon.name: "view-refresh"
                        display: AbstractButton.IconOnly
                        text: "Reload the package list"
                        ToolTip.visible: hovered
                        ToolTip.delay: Kirigami.Units.toolTipDelay
                        ToolTip.text: text
                        onClicked: packageSearch.loadIndex(true)
                    }
                }

                // Shown while the package list is prepared
                Rectangle {
                    id: indexingCard
                    Layout.fillWidth: true
                    visible: packageSearch !== null && packageSearch.isIndexing
                    implicitHeight: indexingLayout.implicitHeight + Kirigami.Units.largeSpacing * 2
                    radius: Kirigami.Units.cornerRadius * 2
                    color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.07)
                    border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.3)

                    RowLayout {
                        id: indexingLayout
                        anchors.fill: parent
                        anchors.margins: Kirigami.Units.largeSpacing
                        spacing: Kirigami.Units.largeSpacing

                        BusyIndicator {
                            Layout.alignment: Qt.AlignTop
                            implicitWidth: Kirigami.Units.iconSizes.large
                            implicitHeight: implicitWidth
                            running: indexingCard.visible
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: Kirigami.Units.smallSpacing

                            Kirigami.Heading {
                                Layout.fillWidth: true
                                level: 3
                                text: packageSearch && packageSearch.evaluating ? "Building the package index" : "Loading the package list"
                            }

                            Label {
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: Kirigami.Theme.disabledTextColor
                                text: packageSearch && packageSearch.evaluating ? "Nix evaluates all packages of nixpkgs once, which takes a minute or two. After that, searching is instant." : "Reading the packages of nixpkgs…"
                            }

                            ProgressBar {
                                Layout.fillWidth: true
                                Layout.topMargin: Kirigami.Units.smallSpacing
                                from: 0
                                to: 1
                                value: packageSearch ? packageSearch.indexProgress : 0
                                indeterminate: !packageSearch || !packageSearch.evaluating
                            }

                            MetaLabel {
                                Layout.fillWidth: true
                                text: packageSearch ? packageSearch.indexStatus : ""
                                font.family: Kirigami.Theme.fixedWidthFont.family
                                font.pointSize: Kirigami.Theme.smallFont.pointSize
                            }
                        }

                        Button {
                            Layout.alignment: Qt.AlignTop
                            text: "Cancel"
                            icon.name: "dialog-cancel"
                            flat: true
                            onClicked: packageSearch.cancelIndexing()
                        }
                    }
                }

                Kirigami.InlineMessage {
                    Layout.fillWidth: true
                    visible: packageSearch !== null && packageSearch.indexError !== "" && !packageSearch.isIndexing
                    type: Kirigami.MessageType.Error
                    text: packageSearch ? "The package list couldn't be loaded: " + packageSearch.indexError : ""
                    actions: [
                        Kirigami.Action {
                            text: "Try Again"
                            icon.name: "view-refresh"
                            onTriggered: packageSearch.loadIndex(true)
                        }
                    ]
                }

                ListFrame {
                    ScrollView {
                        anchors.fill: parent
                        anchors.margins: 1
                        clip: true

                        ListView {
                            id: searchListView
                            model: packageSearch ? packageSearch.results : []
                            activeFocusOnTab: true
                            keyNavigationEnabled: true
                            currentIndex: -1
                            boundsBehavior: Flickable.StopAtBounds
                            Keys.onUpPressed: event => {
                                if (currentIndex <= 0)
                                    searchField.forceActiveFocus();
                                else
                                    event.accepted = false;
                            }
                            onModelChanged: positionViewAtBeginning()

                            delegate: ItemDelegate {
                                id: searchDelegate
                                required property int index
                                required property var modelData

                                readonly property string attrName: modelData.attrName
                                readonly property bool installed: modelData.installed
                                readonly property bool installing: packagesDialog.busy && packagesDialog.operation === "install" && packagesDialog.operationTargets.indexOf(attrName) !== -1
                                // Installed other than with nix profile add, e.g. by the system configuration
                                readonly property string installedBy: modelData.installedBy || ""
                                readonly property bool sameVersionInstalled: modelData.installedVersion === modelData.version
                                readonly property string installedByText: installedBy === "" ? "" : sameVersionInstalled ? "Version " + modelData.version + " is already installed by " + modelData.installedByDescription + "." : modelData.installedByDescription.charAt(0).toUpperCase() + modelData.installedByDescription.slice(1) + " installs version " + modelData.installedVersion + ", nixpkgs has " + modelData.version + "."

                                width: ListView.view.width
                                padding: Kirigami.Units.largeSpacing
                                topPadding: Kirigami.Units.mediumSpacing
                                bottomPadding: Kirigami.Units.mediumSpacing
                                hoverEnabled: true
                                down: false
                                highlighted: ListView.isCurrentItem && searchListView.activeFocus
                                Accessible.name: attrName + " " + modelData.version + (installed ? ", installed" : "")

                                Keys.onReturnPressed: {
                                    if (!installed && !packagesDialog.busy)
                                        packageManager.install(attrName);
                                }

                                background: Rectangle {
                                    color: searchDelegate.installing ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.1) : searchDelegate.highlighted ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.18) : searchDelegate.hovered ? Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.08) : "transparent"

                                    Behavior on color {
                                        ColorAnimation {
                                            duration: Kirigami.Units.shortDuration
                                        }
                                    }

                                    Kirigami.Separator {
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.bottom: parent.bottom
                                        visible: searchDelegate.index < searchListView.count - 1
                                        opacity: 0.6
                                    }
                                }

                                contentItem: RowLayout {
                                    spacing: Kirigami.Units.largeSpacing

                                    PackageIcon {
                                        Layout.alignment: Qt.AlignTop
                                        Layout.topMargin: 2
                                        name: searchDelegate.attrName
                                        iconNames: [searchDelegate.modelData.pname, searchDelegate.attrName.split(".").pop()]
                                    }

                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        spacing: Kirigami.Units.smallSpacing

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Label {
                                                Layout.minimumWidth: Math.min(implicitWidth, Kirigami.Units.gridUnit * 4)
                                                text: searchDelegate.attrName
                                                textFormat: Text.PlainText
                                                font.weight: Font.DemiBold
                                                elide: Text.ElideRight
                                            }

                                            VersionChip {
                                                Layout.maximumWidth: Kirigami.Units.gridUnit * 8
                                                text: searchDelegate.modelData.version
                                            }

                                            Badge {
                                                visible: searchDelegate.installed
                                                text: "Installed"
                                                tint: Kirigami.Theme.positiveTextColor
                                            }

                                            // Installed by e.g. the system configuration
                                            Badge {
                                                visible: searchDelegate.installedBy !== ""
                                                text: "In " + searchDelegate.installedBy + (searchDelegate.sameVersionInstalled ? "" : " · " + searchDelegate.modelData.installedVersion)
                                                tint: Kirigami.Theme.linkColor

                                                HoverHandler {
                                                    id: installedByHover
                                                }

                                                ToolTip.visible: installedByHover.hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                ToolTip.text: searchDelegate.installedByText
                                            }

                                            Item {
                                                Layout.fillWidth: true
                                            }
                                        }

                                        Label {
                                            Layout.fillWidth: true
                                            visible: !searchDelegate.installing
                                            text: searchDelegate.modelData.description !== "" ? searchDelegate.modelData.description : "No description"
                                            textFormat: Text.PlainText
                                            wrapMode: Text.Wrap
                                            maximumLineCount: 2
                                            elide: Text.ElideRight
                                            color: searchDelegate.modelData.description !== "" ? Kirigami.Theme.textColor : Kirigami.Theme.disabledTextColor
                                            font.italic: searchDelegate.modelData.description === ""
                                        }

                                        RowLayout {
                                            Layout.fillWidth: true
                                            visible: searchDelegate.installing
                                            spacing: Kirigami.Units.smallSpacing

                                            BusyIndicator {
                                                implicitWidth: Kirigami.Units.iconSizes.small
                                                implicitHeight: implicitWidth
                                                padding: 0
                                                running: searchDelegate.installing
                                            }

                                            Label {
                                                Layout.fillWidth: true
                                                text: packageManager ? packageManager.operationStatus : ""
                                                font: Kirigami.Theme.smallFont
                                                color: Kirigami.Theme.highlightColor
                                                elide: Text.ElideRight
                                            }
                                        }
                                    }

                                    Button {
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: !searchDelegate.installed && !searchDelegate.installing
                                        text: "Install"
                                        icon.name: "download"
                                        // Installing another copy is rarely needed
                                        flat: searchDelegate.installedBy !== ""
                                        enabled: !packagesDialog.busy
                                        ToolTip.visible: hovered
                                        ToolTip.delay: Kirigami.Units.toolTipDelay
                                        ToolTip.text: (searchDelegate.installedBy !== "" ? searchDelegate.installedByText + " Installing it adds a separate copy to your profile.\n" : "") + "nix profile add nixpkgs#" + searchDelegate.attrName
                                        onClicked: packageManager.install(searchDelegate.attrName)
                                    }

                                    Button {
                                        Layout.alignment: Qt.AlignVCenter
                                        visible: searchDelegate.installed
                                        text: "Installed"
                                        icon.name: "checkmark"
                                        flat: true
                                        ToolTip.visible: hovered
                                        ToolTip.delay: Kirigami.Units.toolTipDelay
                                        ToolTip.text: "Show it in the installed packages"
                                        onClicked: {
                                            filterField.text = searchDelegate.attrName.split(".").pop();
                                            packagesDialog.currentTab = packagesDialog.installedTab;
                                        }
                                    }
                                }

                                TapHandler {
                                    acceptedButtons: Qt.RightButton
                                    onTapped: searchMenu.popup()
                                }

                                Menu {
                                    id: searchMenu

                                    MenuItem {
                                        text: "Install"
                                        icon.name: "download"
                                        enabled: !searchDelegate.installed && !packagesDialog.busy
                                        onTriggered: packageManager.install(searchDelegate.attrName)
                                    }

                                    MenuItem {
                                        text: "Show on search.nixos.org"
                                        icon.name: "globe"
                                        onTriggered: Qt.openUrlExternally(packagesDialog.searchUrl(searchDelegate.attrName))
                                    }

                                    MenuSeparator {}

                                    MenuItem {
                                        text: "Copy Install Command"
                                        icon.name: "edit-copy"
                                        onTriggered: packagesDialog.copyToClipboard("nix profile add nixpkgs#" + searchDelegate.attrName)
                                    }

                                    MenuItem {
                                        text: "Copy Run Command"
                                        icon.name: "edit-copy"
                                        onTriggered: packagesDialog.copyToClipboard("nix run nixpkgs#" + searchDelegate.attrName)
                                    }
                                }
                            }
                        }
                    }

                    Kirigami.PlaceholderMessage {
                        anchors.centerIn: parent
                        width: parent.width - (Kirigami.Units.largeSpacing * 4)
                        visible: searchListView.count === 0 && !(packageSearch && packageSearch.isIndexing && searchField.text.trim() === "")
                        icon.name: "search"
                        text: {
                            if (searchField.text.trim() === "")
                                return "Find packages in nixpkgs";
                            if (packageSearch && packageSearch.isIndexing)
                                return "Searching as soon as the package list is ready…";
                            return "No packages found";
                        }
                        explanation: {
                            if (searchField.text.trim() === "")
                                return "Packages are installed for your user with nix profile add. They don't change your system configuration, and can be updated or removed at any time.";
                            if (packageSearch && packageSearch.isIndexing)
                                return "";
                            return "Try a different name, or describe what the program does, e.g. \"json processor\".";
                        }
                    }
                }
            }
        }

        // Progress of an install, update or removal
        Rectangle {
            id: operationCard
            Layout.fillWidth: true
            visible: packagesDialog.busy
            implicitHeight: operationLayout.implicitHeight + Kirigami.Units.smallSpacing * 2
            radius: Kirigami.Units.cornerRadius * 2
            color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.1)
            border.color: Kirigami.ColorUtils.linearInterpolation(Kirigami.Theme.backgroundColor, Kirigami.Theme.highlightColor, 0.35)

            RowLayout {
                id: operationLayout
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                anchors.leftMargin: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                BusyIndicator {
                    implicitWidth: Kirigami.Units.iconSizes.smallMedium + Kirigami.Units.smallSpacing * 2
                    implicitHeight: implicitWidth
                    padding: Kirigami.Units.smallSpacing
                    running: operationCard.visible
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        text: packagesDialog.operationTitle()
                        font.weight: Font.DemiBold
                        elide: Text.ElideRight
                    }

                    MetaLabel {
                        Layout.fillWidth: true
                        text: packageManager ? packageManager.operationStatus : ""
                    }
                }

                ToolButton {
                    icon.name: "utilities-terminal"
                    display: AbstractButton.IconOnly
                    checkable: true
                    checked: packagesDialog.outputVisible
                    text: packagesDialog.outputVisible ? "Hide output" : "Show output"
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    ToolTip.text: text
                    onClicked: packagesDialog.outputVisible = !packagesDialog.outputVisible
                }

                Button {
                    text: "Cancel"
                    icon.name: "dialog-cancel"
                    flat: true
                    onClicked: packageManager.cancelOperation()
                }
            }
        }

        Kirigami.InlineMessage {
            id: resultMessageBox
            Layout.fillWidth: true
            visible: false
            showCloseButton: true
            type: packagesDialog.resultSuccess ? Kirigami.MessageType.Positive : packagesDialog.resultHint !== "" ? Kirigami.MessageType.Warning : Kirigami.MessageType.Error
            text: packagesDialog.resultMessage
            actions: [
                Kirigami.Action {
                    visible: packagesDialog.resultHint === "unfree"
                    text: packagesDialog.resultOperation === "install" ? "Allow Unfree and Install" : "Allow Unfree and Update"
                    icon.name: "dialog-ok-apply"
                    enabled: !packagesDialog.busy
                    tooltip: "Run nix with NIXPKGS_ALLOW_UNFREE=1 for this package"
                    onTriggered: packageManager.retry("unfree")
                },
                Kirigami.Action {
                    visible: packagesDialog.resultHint === "priority"
                    text: "Install Anyway"
                    icon.name: "dialog-ok-apply"
                    enabled: !packagesDialog.busy
                    tooltip: "Install it with a lower priority, so the installed package keeps providing the file"
                    onTriggered: packageManager.retry("priority")
                },
                Kirigami.Action {
                    visible: !packagesDialog.resultSuccess
                    text: packagesDialog.outputVisible ? "Hide Output" : "Show Output"
                    icon.name: "utilities-terminal"
                    onTriggered: packagesDialog.outputVisible = !packagesDialog.outputVisible
                }
            ]
        }

        // Output of nix, like in a terminal
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 9
            visible: packagesDialog.outputVisible && (packagesDialog.busy || resultMessageBox.visible)
            color: "#0d1117"
            border.color: "#30363d"
            radius: Kirigami.Units.cornerRadius

            ScrollView {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.smallSpacing
                clip: true

                TextArea {
                    id: outputArea
                    readOnly: true
                    selectByMouse: true
                    wrapMode: TextEdit.Wrap
                    font.family: Kirigami.Theme.fixedWidthFont.family
                    font.pointSize: Kirigami.Theme.smallFont.pointSize
                    color: "#c9d1d9"
                    selectionColor: "#264f78"
                    selectedTextColor: "#ffffff"
                    background: null
                    text: packageManager ? packageManager.operationLog : ""
                    onTextChanged: cursorPosition = length
                }
            }
        }
    }

    footer: DialogButtonBox {
        padding: Kirigami.Units.largeSpacing
        topPadding: 0
        alignment: Qt.AlignRight

        Button {
            text: "Close"
            icon.name: "dialog-close"
            DialogButtonBox.buttonRole: DialogButtonBox.RejectRole
        }
    }
}
