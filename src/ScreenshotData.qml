import QtQuick
import QtQml.Models

// In-memory fixtures for the real UI. No commands, settings, network access or
// user-profile writes are possible through these objects.
QtObject {
    id: demo
    readonly property double now: Date.now()

    property QtObject gitManager: QtObject {
        property string repositoryUrl: "https://github.com/example/nixos-config.git"
        property string localPath: "/home/demo/Projects/nixos-config"
        property string status: "3 new commits available"
        property bool isBusy: false
        property int commitsBehind: 3
        property string latestFetchedRevision: "a81b9c2e5678901234567890123456789012345678"
        property string localRevision: "c43e7f15678901234567890123456789012345678"
        property int fetchIntervalMinutes: 5
        property int progress: 100
        property bool isUrlFromGlobalSettings: false
        property var commitsBehindList: [
            {
                sha: latestFetchedRevision,
                message: "Update desktop packages",
                body: "Refresh nixpkgs and the Plasma desktop.\nIncludes the latest security fixes.",
                author: "Alex",
                authorEmail: "alex@example.org",
                timestamp: demo.now - 3600000,
                date: "Today",
                parentCount: 1,
                filesChanged: 2,
                insertions: 24,
                deletions: 18
            },
            {
                sha: "b92c0d35678901234567890123456789012345678",
                message: "Add a remote build host",
                body: "Build the workstation configuration on the build server.",
                author: "Sam",
                authorEmail: "sam@example.org",
                timestamp: demo.now - 86400000,
                date: "Yesterday",
                parentCount: 1,
                filesChanged: 3,
                insertions: 42,
                deletions: 4
            },
            {
                sha: "d54f8a25678901234567890123456789012345678",
                message: "Enable automatic store optimisation",
                body: "Keep shared store paths deduplicated.",
                author: "Alex",
                authorEmail: "alex@example.org",
                timestamp: demo.now - 172800000,
                date: "Two days ago",
                parentCount: 1,
                filesChanged: 1,
                insertions: 6,
                deletions: 0
            }
        ]
        signal operationCompleted(bool success, string message)
        signal pullCompletedForUpdate
        function checkForUpdates() {
        }
        function pullRepository() {
        }
        function cloneOrPullRepository() {
        }
        function ignoreUpdate() {
        }
    }

    property QtObject settingsManager: QtObject {
        property bool startHidden: false
        property bool autostartEnabled: true
        property string hostname: "workstation"
        property int windowWidth: 1100
        property int windowHeight: 900
        property int windowX: 0
        property int windowY: 0
        property string buildHost: ""
        property var buildHosts: ["Build server", "Laptop"]
        property string selectedBuildHost: "Build server"
        property string selectedSwitchHost: ""
        property string selectedBootHost: ""
        property int maxStoredLogs: 10
        property string notificationCommand: "notify-send Nixbit \"$(cat)\""
        property bool notificationCommandOverridden: false
        property bool debugMode: false
        property bool useEmbeddedTerminal: false
        property bool useNh: false
        property bool useNhOverridden: false
        function getBuildHostAddress(name) {
            return name === "Build server" ? "builder@build-server" : "demo@laptop";
        }
        function getLogDirectory() {
            return "/home/demo/.local/share/pbek/nixbit/logs";
        }
        function setAutostartEnabled(enabled) {
            autostartEnabled = enabled;
        }
        function checkAndCreateAutostart() {
        }
        function addBuildHost(name, address) {
        }
        function removeBuildHost(name) {
        }
        function updateBuildHost(oldName, name, address) {
        }
    }

    property QtObject processManager: QtObject {
        property string output: "$ nixos-rebuild build --flake .#workstation\nBuilding the system configuration…\nthese 3 derivations will be built:\n  /nix/store/demo-nixos-system-workstation.drv\ncopying path from https://cache.nixos.org…\nbuilding '/nix/store/demo-system-path.drv'…\nBuild completed successfully.\nResult: /nix/store/demo-nixos-system-workstation"
        property bool isRunning: false
        property bool isPaused: false
        property int lastExitCode: 0
        property bool hasFinished: true
        property int maxOutputLines: 5000
        property bool terminalAvailable: false
        property bool terminalMode: false
        property bool nhAvailable: true
        property bool useNh: false
        property bool waitingForPassword: false
        signal commandFinished(int exitCode, string outputFilePath)
        signal terminalCommandRequested(int runId, string program, var arguments, string workingDirectory)
        signal terminalCloseRequested(int runId)
        signal terminalResetRequested
        signal pauseRefused(string reason)
        function getHostname() {
            return "workstation";
        }
        function detectPrivilegeEscalationTool() {
            return "pkexec";
        }
        function startDetached(program, args, directory) {
            return false;
        }
        function runCommand(program, args) {
        }
        function runCommandInDirectory(program, args, directory) {
        }
        function runNixosRebuildBuild(path, host, builder) {
        }
        function runNixosRebuildSwitch(path, host, builder) {
        }
        function runNixosRebuildBoot(path, host, builder) {
        }
        function terminalCommandStarted(runId, pid) {
        }
        function terminalCommandFinished(runId, code, text, error) {
        }
        function generateTestOutput(count) {
        }
        function clearOutput() {
        }
        function killProcess() {
        }
        function pauseProcess() {
        }
        function resumeProcess() {
        }
    }

    property QtObject systemMonitor: QtObject {
        property double cpuUsage: 68.4
        property double memoryUsage: 42.5
        property string totalMemory: "32.0 GiB"
        property string usedMemory: "13.6 GiB"
        property string networkStats: "↓ 12.4 MiB/s  ↑ 240 KiB/s"
        property string diskStats: "↓ 24.8 MiB/s  ↑ 8.2 MiB/s"
        property double systemLoad: 5.24
        property bool active: false
    }

    property QtObject systemUpdateStatus: QtObject {
        property bool isUpToDate: false
        property bool needsActivation: true
        property string summary: "Switch needed"
        property string details: "The running system was built from an earlier configuration revision."
        property string runningCommit: demo.gitManager.localRevision
        signal statusChanged
        function refresh() {
        }
    }

    property QtObject generationManager: ListModel {
        property bool isLoading: false
        property string error: ""
        property int currentGenerationNumber: 142
        property string currentGenerationDate: "17 Nov 2026, 09:42"
        property int bootGenerationNumber: 143
        signal generationsLoaded
        signal currentGenerationChanged
        function loadGenerations() {
        }
        function setActive(active) {
        }
        ListElement {
            number: 143
            dateTime: "17 Nov 2026, 10:15"
            isCurrent: false
            isBootDefault: true
            nixosVersion: "26.11"
            kernelVersion: "6.18.2"
            commit: "a81b9c2e5678901234567890123456789012345678"
            revisionLabel: "a81b9c2"
        }
        ListElement {
            number: 142
            dateTime: "17 Nov 2026, 09:42"
            isCurrent: true
            isBootDefault: false
            nixosVersion: "26.11"
            kernelVersion: "6.18.1"
            commit: "c43e7f15678901234567890123456789012345678"
            revisionLabel: "c43e7f1"
        }
        ListElement {
            number: 141
            dateTime: "14 Nov 2026, 18:30"
            isCurrent: false
            isBootDefault: false
            nixosVersion: "26.11"
            kernelVersion: "6.18.1"
            commit: "d54f8a25678901234567890123456789012345678"
            revisionLabel: "d54f8a2"
        }
        ListElement {
            number: 140
            dateTime: "10 Nov 2026, 08:05"
            isCurrent: false
            isBootDefault: false
            nixosVersion: "26.11"
            kernelVersion: "6.17.9"
            commit: "e65a9b35678901234567890123456789012345678"
            revisionLabel: "e65a9b3"
        }
    }

    function packageData(name, version, description, size, newVersion) {
        return {
            name: name,
            attrName: name,
            attrPath: "legacyPackages.x86_64-linux." + name,
            originalUrl: "flake:nixpkgs",
            source: "nixpkgs",
            revision: "c43e7f1",
            revisionUrl: "https://github.com/NixOS/nixpkgs/commit/c43e7f1",
            installCommand: "nix profile add nixpkgs#" + name,
            storePaths: ["/nix/store/00000000000000000000000000000000-" + name + "-" + version],
            outputs: ["out"],
            active: true,
            priority: 5,
            metadataLoaded: true,
            missing: false,
            pname: name,
            version: version,
            description: description,
            longDescription: description,
            homepage: "https://github.com/" + (name === "ripgrep" ? "BurntSushi/ripgrep" : name === "jq" ? "jqlang/jq" : "sharkdp/" + name),
            mainProgram: name === "ripgrep" ? "rg" : name,
            licenses: ["MIT"],
            unfree: false,
            insecure: false,
            broken: false,
            closureSize: size,
            installedAt: now - 14 * 86400000,
            installedBeforeHistory: false,
            updatedAt: now - 2 * 86400000,
            updateState: newVersion ? "available" : "upToDate",
            newVersion: newVersion,
            isRebuild: false,
            latestRevision: "a81b9c2",
            latestRevisionUrl: "https://github.com/NixOS/nixpkgs/commit/a81b9c2"
        };
    }

    property QtObject packageManager: QtObject {
        property bool nixAvailable: true
        property string profilePath: "/home/demo/.local/state/nix/profiles/profile"
        property var packages: [demo.packageData("ripgrep", "14.1.0", "Search tool combining the usability of Ag with the speed of grep", 18600000, "14.1.1"), demo.packageData("bat", "0.24.0", "A cat clone with syntax highlighting and Git integration", 42300000, "0.25.0"), demo.packageData("jq", "1.7.1", "Lightweight and flexible command-line JSON processor", 12500000, ""), demo.packageData("fd", "10.2.0", "A simple, fast and user-friendly alternative to find", 16800000, "")]
        property int packageCount: packages.length
        property var otherEntries: ["home-manager-path"]
        property var installedAttrNames: ["ripgrep", "bat", "jq", "fd"]
        property double totalSize: 78300000
        property bool isLoading: false
        property bool loaded: true
        property string error: ""
        property bool isCheckingUpdates: false
        property string updateCheckStatus: ""
        property string updateCheckError: ""
        property double lastUpdateCheck: demo.now - 180000
        property int updateCount: 2
        property bool isBusy: false
        property string operation: ""
        property var operationTargets: []
        property string operationStatus: ""
        property string operationLog: ""
        property string activityText: "Updating ripgrep"
        signal operationFinished(bool success, string operation, var targets, string message, string hint)
        function themeIconName(candidates) {
            return "package-x-generic";
        }
        function refresh() {
        }
        function checkForUpdates() {
        }
        function checkForUpdatesIfStale(age) {
        }
        function install(name, allowUnfree, priority) {
        }
        function upgrade(names, allowUnfree) {
        }
        function upgradeAll(allowUnfree) {
        }
        function remove(names) {
        }
        function cancelOperation() {
        }
        function retry(hint) {
        }
    }

    property QtObject packageSearch: QtObject {
        property bool isIndexing: false
        property bool indexReady: true
        property double indexProgress: 1
        property string indexStatus: ""
        property bool evaluating: false
        property string indexError: ""
        property int indexSize: 124382
        property string nixpkgsVersion: "26.11pre-git"
        property string query: "git"
        property var results: [
            {
                attrName: "git",
                pname: "git",
                version: "2.51.0",
                description: "Distributed version control system",
                installed: false,
                installedBy: "system",
                installedByDescription: "the system configuration",
                installedVersion: "2.51.0"
            },
            {
                attrName: "git-lfs",
                pname: "git-lfs",
                version: "3.7.0",
                description: "Git extension for versioning large files",
                installed: false
            },
            {
                attrName: "gitui",
                pname: "gitui",
                version: "0.27.0",
                description: "Blazing fast terminal UI for Git",
                installed: false
            },
            {
                attrName: "lazygit",
                pname: "lazygit",
                version: "0.55.0",
                description: "Simple terminal UI for Git commands",
                installed: false
            },
            {
                attrName: "tig",
                pname: "tig",
                version: "2.5.12",
                description: "Text-mode interface for Git",
                installed: false
            }
        ]
        property int matchCount: 5
        signal indexChanged
        function loadIndex(force) {
        }
        function search(text) {
            query = text;
        }
        function cancelIndexing() {
        }
    }

    property QtObject logManager: QtObject {
        property var logFiles: [
            {
                displayName: "17 Nov 2026, 09:42",
                filePath: "/home/demo/build.log",
                exitCode: 0,
                buildType: "switch",
                fileSize: 84210
            },
            {
                displayName: "17 Nov 2026, 09:35",
                filePath: "/home/demo/build.log",
                exitCode: 0,
                buildType: "build",
                fileSize: 128430
            },
            {
                displayName: "16 Nov 2026, 18:20",
                filePath: "/home/demo/build.log",
                exitCode: 1,
                buildType: "build",
                fileSize: 12480
            },
            {
                displayName: "14 Nov 2026, 18:30",
                filePath: "/home/demo/build.log",
                exitCode: 0,
                buildType: "boot",
                fileSize: 72340
            }
        ]
        function refreshLogs(directory) {
        }
        function openLogInEditor(path) {
        }
        function deleteLog(path) {
        }
        function saveLog(path, code, directory, maxLogs, type) {
        }
    }
}
