#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QElapsedTimer>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTemporaryFile>
#include <QTimer>

class ProcessManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString output READ output NOTIFY outputChanged)
  Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
  Q_PROPERTY(bool isPaused READ isPaused NOTIFY isPausedChanged)
  Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged)
  Q_PROPERTY(bool hasFinished READ hasFinished NOTIFY hasFinishedChanged)
  Q_PROPERTY(int maxOutputLines READ maxOutputLines WRITE setMaxOutputLines
                 NOTIFY maxOutputLinesChanged)

public:
  explicit ProcessManager(QObject *parent = nullptr);
  ~ProcessManager();

  QString output() const { return m_output; }
  bool isRunning() const { return m_isRunning; }
  bool isPaused() const { return m_isPaused; }
  int lastExitCode() const { return m_lastExitCode; }
  bool hasFinished() const { return m_hasFinished; }
  int maxOutputLines() const { return m_maxOutputLines; }
  void setMaxOutputLines(int lines);
  void setNotificationCommand(const QString &command);

  Q_INVOKABLE void runCommand(const QString &program,
                              const QStringList &arguments = QStringList());
  Q_INVOKABLE void runCommandInDirectory(const QString &program,
                                         const QStringList &arguments,
                                         const QString &workingDirectory);
  Q_INVOKABLE void killProcess();
  Q_INVOKABLE QString getHostname();
  Q_INVOKABLE bool startDetached(const QString &program,
                                 const QStringList &arguments = QStringList(),
                                 const QString &workingDirectory = QString());
  Q_INVOKABLE void clearOutput();
  Q_INVOKABLE void pauseProcess();
  Q_INVOKABLE void resumeProcess();
  Q_INVOKABLE void runNixosRebuildBuild(const QString &repoPath,
                                        const QString &hostname,
                                        const QString &buildHost = QString());
  Q_INVOKABLE void runNixosRebuildSwitch(const QString &repoPath,
                                         const QString &hostname,
                                         const QString &buildHost = QString());
  Q_INVOKABLE void runNixosRebuildBoot(const QString &repoPath,
                                       const QString &hostname,
                                       const QString &buildHost = QString());
  Q_INVOKABLE void generateTestOutput(int lineCount);

  // Pre-flight check for privileged rebuilds. Returns a short, human-readable
  // description of which privilege escalation tool would be used for a switch
  // or boot rebuild (for example "pkexec", "sudo (with askpass)"), or an empty
  // string if no usable setuid tool is found.
  Q_INVOKABLE QString detectPrivilegeEscalationTool() const;

signals:
  void outputChanged();
  void isRunningChanged();
  void isPausedChanged();
  void lastExitCodeChanged();
  void hasFinishedChanged();
  void maxOutputLinesChanged();
  void commandFinished(int exitCode, const QString &outputFilePath);

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
  void flushPendingOutput();

private:
  void appendOutput(const QString &text);
  void setIsRunning(bool running);
  void setIsPaused(bool paused);
  void trimOutputToLimit();
  void openOutputLogFile();
  void closeOutputLogFile();
  void runPrivilegedRebuild(const QString &mode, const QString &repoPath,
                            const QString &hostname, const QString &buildHost);
  void sendRebuildNotification(const QString &mode, const QString &hostname,
                               int exitCode, qint64 elapsedMilliseconds);

  // Result of resolving a privilege escalation tool in C++ (used both for the
  // pre-flight check and to build the rebuild command deterministically).
  struct EscalationTool {
    QString command;     // e.g. "/run/wrappers/bin/pkexec" or
                         // "/run/wrappers/bin/sudo -A"; empty if none found
    QString description; // human-readable, e.g. "pkexec", "sudo (with askpass)"
    QString askpass; // path to a graphical askpass helper for sudo -A, if any
    bool usesPkexec = false;
    bool found() const { return !command.isEmpty(); }
  };
  EscalationTool resolveEscalationTool() const;

  QProcess *m_process;
  QString m_output; // Truncated output for UI display
  QStringList m_outputLines;
  bool m_isRunning;
  bool m_isPaused;
  int m_lastExitCode;
  bool m_hasFinished;
  int m_maxOutputLines;
  QString m_notificationCommand;
  QString m_rebuildMode;
  QString m_rebuildHostname;
  QElapsedTimer m_rebuildTimer;

  // Stream full output to a temporary file instead of holding in memory
  QTemporaryFile *m_outputLogFile;

  // Output batching for reduced UI updates
  QTimer *m_outputFlushTimer;
  QString m_pendingOutput;
  bool m_hasPendingOutput;
};

#endif // PROCESSMANAGER_H
