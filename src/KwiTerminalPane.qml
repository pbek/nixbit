// Embedded Kwi terminal, loaded via a Loader in main.qml so that Nixbit keeps
// working with the plain text output view when the optional
// org.kde.kwi.terminal QML module is not installed
import QtQuick
import QtQuick.Controls
import org.kde.kwi.terminal as Kwi

Item {
    id: pane

    readonly property bool hasSession: terminal.session !== null
    readonly property var view: terminal.view
    property int scrollbackLines: -1
    property int currentRunId: -1

    function run(runId, program, args, workingDirectory) {
        reset();
        currentRunId = runId;

        var session = sessionComponent.createObject(pane, {
            executable: program,
            arguments: args,
            initialDirectory: workingDirectory,
            scrollbackLines: Math.min(Math.max(pane.scrollbackLines, -1), 100000)
        });

        session.started.connect(function () {
            processManager.terminalCommandStarted(runId, session.processId);
        });
        session.exited.connect(function (code) {
            processManager.terminalCommandFinished(runId, code, session.historyText(), session.errorString);

            // Nothing useful is shown when the launch failed, so reveal the
            // error in the plain text output view instead
            if (session.state === Kwi.Session.Failed) {
                Qt.callLater(function () {
                    if (pane.currentRunId === runId) {
                        pane.reset();
                    }
                });
            }
        });

        terminal.session = session;
        terminal.forceActiveFocus();

        // A rejected launch doesn't emit exited(), report it ourselves
        if (!session.start() && !session.finished) {
            processManager.terminalCommandFinished(runId, 127, "", session.errorString || "Failed to start the terminal process");
            reset();
        }
    }

    function closeSession(runId) {
        if (terminal.session && runId === currentRunId) {
            terminal.session.close();
        }
    }

    function reset() {
        var session = terminal.session;
        terminal.session = null;
        currentRunId = -1;
        if (session) {
            session.close();
            session.destroy();
        }
    }

    Component {
        id: sessionComponent
        Kwi.Session {}
    }

    Kwi.Terminal {
        id: terminal
        anchors.fill: parent

        // Kwi's bundled dark Breeze scheme, matching the dark plain text view
        Component.onCompleted: view.colorScheme = "dark"

        onContextMenuRequested: position => contextMenu.popup()
        onLinkActivated: url => Qt.openUrlExternally(url)
    }

    Menu {
        id: contextMenu

        MenuItem {
            text: "Copy"
            icon.name: "edit-copy"
            enabled: terminal.view.hasSelection
            onTriggered: terminal.view.copy()
        }

        MenuItem {
            text: "Select All"
            icon.name: "edit-select-all"
            onTriggered: terminal.view.selectAll()
        }
    }
}
