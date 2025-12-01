#include "processmanager.h"
#include <QDebug>
#include <QSysInfo>
#include <csignal>
#include <unistd.h>

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent), m_process(nullptr), m_isRunning(false),
      m_isPaused(false), m_lastExitCode(0), m_hasFinished(false),
      m_maxOutputLines(5000) {
  m_process = new QProcess(this);

  // Set up the process to create a new process group
  // This allows us to send signals to the entire process tree
  m_process->setChildProcessModifier([]() {
    // Create a new process group with the current process as the leader
    setpgid(0, 0);
  });

  connect(m_process, &QProcess::readyReadStandardOutput, this,
          &ProcessManager::onReadyReadStandardOutput);
  connect(m_process, &QProcess::readyReadStandardError, this,
          &ProcessManager::onReadyReadStandardError);
  connect(m_process,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
          &ProcessManager::onProcessFinished);
}

ProcessManager::~ProcessManager() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(3000);
  }
}

void ProcessManager::runCommand(const QString &program,
                                const QStringList &arguments) {
  if (m_isRunning) {
    qDebug() << "Process already running, killing it first";
    killProcess();
  }

  m_output.clear();
  emit outputChanged();

  m_hasFinished = false;
  emit hasFinishedChanged();

  QString commandLine = program;
  if (!arguments.isEmpty()) {
    commandLine += " " + arguments.join(" ");
  }

  appendOutput(QString("Running: %1\n\n").arg(commandLine));

  setIsRunning(true);
  m_process->start(program, arguments);

  if (!m_process->waitForStarted(3000)) {
    appendOutput(QString("\nError: Failed to start process\n"));
    setIsRunning(false);
  }
}

void ProcessManager::runCommandInDirectory(const QString &program,
                                           const QStringList &arguments,
                                           const QString &workingDirectory) {
  if (m_isRunning) {
    qDebug() << "Process already running, killing it first";
    killProcess();
  }

  m_output.clear();
  emit outputChanged();

  QString commandLine = program;
  if (!arguments.isEmpty()) {
    commandLine += " " + arguments.join(" ");
  }

  appendOutput(QString("Working directory: %1\n").arg(workingDirectory));
  appendOutput(QString("Running: %1\n\n").arg(commandLine));

  m_process->setWorkingDirectory(workingDirectory);
  setIsRunning(true);
  m_process->start(program, arguments);

  if (!m_process->waitForStarted(3000)) {
    appendOutput(QString("\nError: Failed to start process\n"));
    setIsRunning(false);
  }
}

void ProcessManager::killProcess() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(1000);
    appendOutput("\n\n=== Process killed ===\n");
  }
}

bool ProcessManager::startDetached(const QString &program,
                                   const QStringList &arguments,
                                   const QString &workingDirectory) {
  qDebug() << "Starting detached process:" << program << arguments;

  bool success;
  if (workingDirectory.isEmpty()) {
    success = QProcess::startDetached(program, arguments);
  } else {
    success = QProcess::startDetached(program, arguments, workingDirectory);
  }

  if (!success) {
    qDebug() << "Failed to start detached process:" << program;
  }

  return success;
}

void ProcessManager::clearOutput() {
  m_output.clear();
  m_outputLines.clear();
  emit outputChanged();
}

QString ProcessManager::getHostname() { return QSysInfo::machineHostName(); }

void ProcessManager::onReadyReadStandardOutput() {
  QString output = QString::fromUtf8(m_process->readAllStandardOutput());
  appendOutput(output);
}

void ProcessManager::onReadyReadStandardError() {
  QString error = QString::fromUtf8(m_process->readAllStandardError());
  appendOutput(error);
}

void ProcessManager::onProcessFinished(int exitCode,
                                       QProcess::ExitStatus exitStatus) {
  setIsRunning(false);

  m_lastExitCode = exitCode;
  emit lastExitCodeChanged();

  m_hasFinished = true;
  emit hasFinishedChanged();

  QString statusText;
  if (exitStatus == QProcess::CrashExit) {
    statusText = QString("\n\n=== Process crashed ===");
  } else {
    statusText = QString("\n\n=== Process finished with exit code: %1 ===")
                     .arg(exitCode);
  }

  appendOutput(statusText);
  emit commandFinished(exitCode, m_output);
}

void ProcessManager::appendOutput(const QString &text) {
  // Split new text into lines and append to buffer
  QStringList newLines = text.split('\n');

  // If we already have lines and the first new item doesn't start a new line,
  // append it to the last existing line
  if (!m_outputLines.isEmpty() && !text.startsWith('\n')) {
    m_outputLines.last() += newLines.first();
    newLines.removeFirst();
  }

  // Add remaining lines
  m_outputLines.append(newLines);

  // Trim to max lines if needed
  trimOutputToLimit();

  // Rebuild the output string from lines
  m_output = m_outputLines.join('\n');
  emit outputChanged();
}

void ProcessManager::trimOutputToLimit() {
  if (m_maxOutputLines > 0 && m_outputLines.size() > m_maxOutputLines) {
    // Keep only the last N lines
    int linesToRemove = m_outputLines.size() - m_maxOutputLines;
    m_outputLines = m_outputLines.mid(linesToRemove);

    // Add indicator that output was truncated
    if (!m_outputLines.isEmpty() &&
        !m_outputLines.first().startsWith("... (output truncated)")) {
      m_outputLines.prepend("... (output truncated, showing last " +
                            QString::number(m_maxOutputLines) +
                            " lines) ...\n");
    }
  }
}

void ProcessManager::setMaxOutputLines(int lines) {
  if (m_maxOutputLines != lines) {
    m_maxOutputLines = lines;
    trimOutputToLimit();
    m_output = m_outputLines.join('\n');
    emit maxOutputLinesChanged();
    emit outputChanged();
  }
}

void ProcessManager::setIsRunning(bool running) {
  if (m_isRunning != running) {
    m_isRunning = running;
    if (!running) {
      m_isPaused = false;
      emit isPausedChanged();
    }
    emit isRunningChanged();
  }
}

void ProcessManager::setIsPaused(bool paused) {
  if (m_isPaused != paused) {
    m_isPaused = paused;
    emit isPausedChanged();
  }
}

void ProcessManager::pauseProcess() {
  if (!m_isRunning || m_isPaused)
    return;
  qint64 pid = m_process->processId();
  if (pid > 0) {
    // Send SIGSTOP to the entire process group, not just the process
    // This ensures all child processes are paused too
    kill(-static_cast<pid_t>(pid), SIGSTOP);
    setIsPaused(true);
    appendOutput("\n\n=== Process paused ===\n");
  }
}

void ProcessManager::resumeProcess() {
  if (!m_isRunning || !m_isPaused)
    return;
  qint64 pid = m_process->processId();
  if (pid > 0) {
    // Send SIGCONT to the entire process group
    kill(-static_cast<pid_t>(pid), SIGCONT);
    setIsPaused(false);
    appendOutput("\n\n=== Process resumed ===\n");
  }
}
