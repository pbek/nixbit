#ifndef PROCESSMANAGER_H
#define PROCESSMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

class ProcessManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString output READ output NOTIFY outputChanged)
  Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
  Q_PROPERTY(bool isPaused READ isPaused NOTIFY isPausedChanged)
  Q_PROPERTY(int lastExitCode READ lastExitCode NOTIFY lastExitCodeChanged)
  Q_PROPERTY(bool hasFinished READ hasFinished NOTIFY hasFinishedChanged)

public:
  explicit ProcessManager(QObject *parent = nullptr);
  ~ProcessManager();

  QString output() const { return m_output; }
  bool isRunning() const { return m_isRunning; }
  bool isPaused() const { return m_isPaused; }
  int lastExitCode() const { return m_lastExitCode; }
  bool hasFinished() const { return m_hasFinished; }

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

signals:
  void outputChanged();
  void isRunningChanged();
  void isPausedChanged();
  void lastExitCodeChanged();
  void hasFinishedChanged();
  void commandFinished(int exitCode, const QString &output);

private slots:
  void onReadyReadStandardOutput();
  void onReadyReadStandardError();
  void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
  void appendOutput(const QString &text);
  void setIsRunning(bool running);
  void setIsPaused(bool paused);

  QProcess *m_process;
  QString m_output;
  bool m_isRunning;
  bool m_isPaused;
  int m_lastExitCode;
  bool m_hasFinished;
};

#endif // PROCESSMANAGER_H
