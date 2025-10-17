import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: root
    visible: true
    width: 800
    height: 600
    title: "Nixbit NixOS Updater - Git Status"

    menuBar: MenuBar {
        Menu {
            title: "&File"
            Action {
                text: "&Refresh"
                onTriggered: terminalOutput.runGitStatus()
            }
            MenuSeparator {}
            Action {
                text: "&Quit"
                onTriggered: Qt.quit()
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 10

        Label {
            text: "Git Status Output"
            font.bold: true
            font.pixelSize: 16
        }

        Button {
            text: "Refresh Git Status"
            Layout.alignment: Qt.AlignHCenter
            onClicked: terminalOutput.runGitStatus()
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#1e1e1e"
            border.color: "#3e3e3e"
            border.width: 1
            radius: 4

            ScrollView {
                anchors.fill: parent
                anchors.margins: 5
                clip: true

                TextArea {
                    id: terminalOutput
                    readOnly: true
                    textFormat: TextEdit.PlainText
                    wrapMode: TextEdit.Wrap
                    font.family: "Monospace"
                    font.pixelSize: 12
                    color: "#00ff00"
                    background: Rectangle {
                        color: "transparent"
                    }

                    property var process: null

                    function runGitStatus() {
                        if (process !== null) {
                            process.kill()
                            process.destroy()
                        }

                        text = "Running 'git status'...\n\n"

                        // Create a QProcess to run git status
                        process = Qt.createQmlObject('
                            import QtQml
                            import QtCore

                            QtObject {
                                id: proc
                                property var qprocess: null

                                signal finished()

                                function start(program, args) {
                                    // Note: QProcess is not directly available in QML in Qt6
                                    // This is a simplified version - in production you would
                                    // expose a QProcess wrapper from C++
                                    console.log("Starting:", program, args.join(" "))
                                    finished()
                                }

                                function kill() {
                                }

                                function readAllStandardOutput() {
                                    return "fatal: not a git repository (or any of the parent directories): .git\\n"
                                }

                                function readAllStandardError() {
                                    return ""
                                }

                                function exitCode() {
                                    return 128
                                }
                            }
                        ', terminalOutput)

                        if (process) {
                            process.finished.connect(function() {
                                var output = process.readAllStandardOutput()
                                var errors = process.readAllStandardError()

                                terminalOutput.text = "=== Git Status ===\n\n"

                                if (output) {
                                    terminalOutput.text += output
                                }
                                if (errors) {
                                    terminalOutput.text += "\n=== Errors ===\n" + errors
                                }

                                if (!output && !errors) {
                                    terminalOutput.text += "(No output)"
                                }

                                terminalOutput.text += "\n\n=== Exit Code: " + process.exitCode() + " ==="
                                terminalOutput.text += "\n\nNote: QProcess is not directly available in Qt6 QML."
                                terminalOutput.text += "\nFor full functionality, a C++ QProcess wrapper should be exposed to QML."
                            })

                            process.start("git", ["status"])
                        } else {
                            text = "Error: Could not create process object\n\n"
                            text += "Note: In Qt6, QProcess needs to be exposed from C++ to QML.\n"
                            text += "This is a demonstration showing the UI layout."
                        }
                    }

                    Component.onCompleted: {
                        runGitStatus()
                    }
                }
            }
        }

        Label {
            text: "Working directory: " + StandardPaths.writableLocation(StandardPaths.HomeLocation)
            font.pixelSize: 10
            color: "gray"
        }
    }
}
