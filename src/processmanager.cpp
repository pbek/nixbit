#include "processmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMultiHash>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTextStream>
#include <QTimer>
#include <algorithm>
#include <csignal>
#include <sys/stat.h>
#include <unistd.h>

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent), m_process(nullptr), m_isRunning(false),
      m_isPaused(false), m_lastExitCode(0), m_hasFinished(false),
      m_maxOutputLines(2000), m_outputLogFile(nullptr),
      m_hasPendingOutput(false), m_terminalAvailable(false),
      m_terminalMode(false), m_terminalRunActive(false), m_terminalRunId(0),
      m_terminalProcessId(0), m_terminalTranscriptFile(nullptr) {
  m_process = new QProcess(this);

  // Set up output batching timer (2 second intervals)
  m_outputFlushTimer = new QTimer(this);
  m_outputFlushTimer->setInterval(2000);
  m_outputFlushTimer->setSingleShot(false);
  connect(m_outputFlushTimer, &QTimer::timeout, this,
          &ProcessManager::flushPendingOutput);

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
  closeOutputLogFile();
}

void ProcessManager::openOutputLogFile() {
  closeOutputLogFile();
  m_outputLogFile = new QTemporaryFile(this);
  m_outputLogFile->setAutoRemove(true);
  if (!m_outputLogFile->open()) {
    qDebug() << "Failed to create temporary log file:"
             << m_outputLogFile->errorString();
    delete m_outputLogFile;
    m_outputLogFile = nullptr;
  }
}

void ProcessManager::closeOutputLogFile() {
  if (m_outputLogFile) {
    m_outputLogFile->close();
    delete m_outputLogFile;
    m_outputLogFile = nullptr;
  }
}

namespace {

// Returns true if the path exists, is a regular file, and has the setuid bit.
bool isSetuidExecutable(const QString &path) {
  if (path.isEmpty()) {
    return false;
  }
  struct stat st;
  if (stat(path.toLocal8Bit().constData(), &st) != 0) {
    return false;
  }
  return S_ISREG(st.st_mode) && (st.st_mode & S_ISUID) != 0;
}

// Resolve a binary either from an absolute path or by searching PATH.
QString resolveBinary(const QString &candidate) {
  if (candidate.startsWith('/')) {
    return candidate;
  }
  return QStandardPaths::findExecutable(candidate);
}

QString formatDuration(qint64 milliseconds) {
  const qint64 totalSeconds = milliseconds / 1000;
  const qint64 hours = totalSeconds / 3600;
  const qint64 minutes = (totalSeconds % 3600) / 60;
  const qint64 seconds = totalSeconds % 60;

  QStringList parts;
  if (hours > 0) {
    parts.append(QString("%1h").arg(hours));
  }
  if (minutes > 0) {
    parts.append(QString("%1m").arg(minutes));
  }
  if (seconds > 0 || parts.isEmpty()) {
    parts.append(QString("%1s").arg(seconds));
  }
  return parts.join(' ');
}

QString shellQuote(const QString &argument) {
  QString quoted = argument;
  quoted.replace('\'', "'\\''");
  return '\'' + quoted + '\'';
}

// Turn a line of raw terminal output into plain text for the log file
QString stripTerminalControls(QString line) {
  static const QRegularExpression osc(
      QStringLiteral("\\x1B\\][^\\x07\\x1B]*(?:\\x07|\\x1B\\\\)"));
  static const QRegularExpression csi(
      QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]"));
  static const QRegularExpression otherEscapes(
      QStringLiteral("\\x1B[()#][0-9A-Za-z]|\\x1B[=>78DEHMNOZc\\\\]|\\x07"));

  line.remove(osc);
  line.remove(csi);
  line.remove(otherEscapes);

  while (line.endsWith('\n') || line.endsWith('\r')) {
    line.chop(1);
  }

  // Carriage returns redraw the line (e.g. status bars), keep the final text
  const qsizetype lastCarriageReturn = line.lastIndexOf('\r');
  if (lastCarriageReturn >= 0) {
    line = line.mid(lastCarriageReturn + 1);
  }

  return line;
}

// Collect all descendant process ids of the given process from /proc
QList<pid_t> descendantProcessIds(pid_t root) {
  QMultiHash<pid_t, pid_t> children;
  const QStringList entries = QDir(QStringLiteral("/proc"))
                                  .entryList(QDir::Dirs | QDir::NoDotAndDotDot);
  for (const QString &entry : entries) {
    bool ok = false;
    const pid_t pid = entry.toInt(&ok);
    if (!ok) {
      continue;
    }
    QFile statFile(QStringLiteral("/proc/%1/stat").arg(pid));
    if (!statFile.open(QIODevice::ReadOnly)) {
      continue;
    }
    // Format: "pid (comm) state ppid ...", comm may contain spaces/parens
    const QByteArray stat = statFile.readAll();
    const qsizetype commEnd = stat.lastIndexOf(')');
    if (commEnd < 0) {
      continue;
    }
    const QList<QByteArray> fields = stat.mid(commEnd + 2).split(' ');
    if (fields.size() > 1) {
      children.insert(fields.at(1).toInt(), pid);
    }
  }

  QList<pid_t> result;
  QList<pid_t> queue{root};
  while (!queue.isEmpty()) {
    const pid_t parent = queue.takeFirst();
    const QList<pid_t> direct = children.values(parent);
    for (pid_t child : direct) {
      if (!result.contains(child)) {
        result.append(child);
        queue.append(child);
      }
    }
  }
  return result;
}

} // namespace

ProcessManager::EscalationTool ProcessManager::resolveEscalationTool() const {
  EscalationTool tool;

  // 1. Prefer a setuid pkexec (graphical authentication dialog).
  const QStringList pkexecCandidates = {"/run/wrappers/bin/pkexec", "pkexec"};
  for (const QString &candidate : pkexecCandidates) {
    const QString resolved = resolveBinary(candidate);
    if (isSetuidExecutable(resolved)) {
      tool.command = resolved;
      tool.description = "pkexec";
      tool.usesPkexec = true;
      return tool;
    }
  }

  // 2. Fall back to a setuid sudo, using a graphical askpass helper if present.
  const QStringList sudoCandidates = {"/run/wrappers/bin/sudo", "sudo"};
  for (const QString &candidate : sudoCandidates) {
    const QString resolved = resolveBinary(candidate);
    if (!isSetuidExecutable(resolved)) {
      continue;
    }

    const QStringList askpassCandidates = {"/run/wrappers/bin/ksshaskpass",
                                           "ksshaskpass", "ssh-askpass",
                                           "lxqt-openssh-askpass"};
    QString askpass;
    for (const QString &a : askpassCandidates) {
      const QString resolvedAskpass = resolveBinary(a);
      if (!resolvedAskpass.isEmpty() &&
          QFileInfo(resolvedAskpass).isExecutable()) {
        askpass = resolvedAskpass;
        break;
      }
    }

    if (!askpass.isEmpty()) {
      tool.command = resolved + " -A";
      tool.description = "sudo (with graphical password prompt)";
      tool.askpass = askpass;
    } else {
      tool.command = resolved;
      tool.description = "sudo (terminal password prompt)";
    }
    return tool;
  }

  // 3. Nothing usable found.
  return tool;
}

QString ProcessManager::detectPrivilegeEscalationTool() const {
  return resolveEscalationTool().description;
}

void ProcessManager::runCommand(const QString &program,
                                const QStringList &arguments) {
  if (m_isRunning) {
    qDebug() << "Process already running, killing it first";
    killProcess();
  }

  m_output.clear();
  m_outputLines.clear();
  m_pendingOutput.clear();
  m_hasPendingOutput = false;
  emit outputChanged();

  // Open a new temp file for streaming the full output
  openOutputLogFile();

  m_hasFinished = false;
  emit hasFinishedChanged();

  QString commandLine = program;
  if (!arguments.isEmpty()) {
    commandLine += " " + arguments.join(" ");
  }

  if (m_terminalMode) {
    // The terminal prints the header itself, so it also ends up in the log
    m_terminalHeader = QString("Running: %1").arg(commandLine);
    setIsRunning(true);
    startTerminalCommand(program, arguments, QString());
    return;
  }

  appendOutput(QString("Running: %1\n\n").arg(commandLine));

  setIsRunning(true);
  m_outputFlushTimer->start(); // Start batching timer
  m_process->start(program, arguments);

  if (!m_process->waitForStarted(3000)) {
    appendOutput(QString("\nError: Failed to start process\n"));
    setIsRunning(false);
    m_outputFlushTimer->stop();

    const QString rebuildMode = m_rebuildMode;
    const QString rebuildHostname = m_rebuildHostname;
    const qint64 rebuildElapsed =
        m_rebuildTimer.isValid() ? m_rebuildTimer.elapsed() : 0;
    m_rebuildMode.clear();
    m_rebuildHostname.clear();
    m_rebuildTimer.invalidate();
    if (!rebuildMode.isEmpty()) {
      sendRebuildNotification(rebuildMode, rebuildHostname, -1, rebuildElapsed);
    }
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
  m_outputLines.clear();
  emit outputChanged();

  // Open a new temp file for streaming the full output
  openOutputLogFile();

  m_hasFinished = false;
  emit hasFinishedChanged();

  m_process->setWorkingDirectory(workingDirectory);
  setIsRunning(true);
  m_process->start(program, arguments);

  if (!m_process->waitForStarted(3000)) {
    appendOutput(QString("\nError: Failed to start process\n"));
    setIsRunning(false);
  }
}

void ProcessManager::onProcessFinished(int exitCode,
                                       QProcess::ExitStatus exitStatus) {
  handleProcessFinished(exitCode, exitStatus == QProcess::CrashExit);
}

void ProcessManager::handleProcessFinished(int exitCode, bool crashed) {
  m_terminalRunActive = false;
  m_terminalProcessId = 0;

  const QString rebuildMode = m_rebuildMode;
  const QString rebuildHostname = m_rebuildHostname;
  const qint64 rebuildElapsed =
      m_rebuildTimer.isValid() ? m_rebuildTimer.elapsed() : 0;
  m_rebuildMode.clear();
  m_rebuildHostname.clear();
  m_rebuildTimer.invalidate();

  // Stop output batching timer
  m_outputFlushTimer->stop();

  // Flush any remaining pending output
  if (m_hasPendingOutput) {
    flushPendingOutput();
  }

  setIsRunning(false);

  m_lastExitCode = exitCode;
  emit lastExitCodeChanged();

  m_hasFinished = true;
  emit hasFinishedChanged();

  QString statusText;
  if (crashed) {
    statusText = QString("\n\n=== Process crashed ===");
  } else {
    statusText = QString("\n\n=== Process finished with exit code: %1 ===")
                     .arg(exitCode);
  }

  appendOutput(statusText);

  if (!rebuildMode.isEmpty()) {
    sendRebuildNotification(rebuildMode, rebuildHostname,
                            crashed ? -1 : exitCode, rebuildElapsed);
  }

  // Emit the temp file path so the log manager can copy it directly to the
  // log directory, avoiding loading the entire output into memory
  QString outputFilePath;
  if (m_outputLogFile) {
    m_outputLogFile->flush();
    outputFilePath = m_outputLogFile->fileName();
  }
  emit commandFinished(exitCode, outputFilePath);

  // Close and delete the temp file after logging is done
  closeOutputLogFile();
}

void ProcessManager::appendOutput(const QString &text) {
  // Stream full output to the temporary file (no memory accumulation)
  if (m_outputLogFile && m_outputLogFile->isOpen()) {
    m_outputLogFile->write(text.toUtf8());
  }

  // If batching is active, accumulate in pending buffer
  if (m_outputFlushTimer->isActive()) {
    m_pendingOutput += text;
    m_hasPendingOutput = true;
    // Don't emit outputChanged yet - wait for timer
    return;
  }

  // If not batching, process immediately (for startup messages)
  // Split new text into lines and append to buffer for UI display
  QStringList newLines = text.split('\n');

  // If we already have lines and the first new item doesn't start a new line,
  // append it to the last existing line
  if (!m_outputLines.isEmpty() && !text.startsWith('\n')) {
    m_outputLines.last() += newLines.first();
    newLines.removeFirst();
  }

  // Add remaining lines
  m_outputLines.append(newLines);

  // Trim to max lines if needed (only for UI display)
  trimOutputToLimit();

  // Rebuild the output string from lines (truncated for UI)
  m_output = m_outputLines.join('\n');
  emit outputChanged();
}

void ProcessManager::flushPendingOutput() {
  if (!m_hasPendingOutput) {
    return;
  }

  // Process all pending output
  QString textToProcess = m_pendingOutput;
  m_pendingOutput.clear();
  m_hasPendingOutput = false;

  // Split new text into lines and append to buffer for UI display
  QStringList newLines = textToProcess.split('\n');

  // If we already have lines and the first new item doesn't start a new line,
  // append it to the last existing line
  if (!m_outputLines.isEmpty() && !textToProcess.startsWith('\n')) {
    m_outputLines.last() += newLines.first();
    newLines.removeFirst();
  }

  // Add remaining lines
  m_outputLines.append(newLines);

  // Trim to max lines if needed (only for UI display)
  trimOutputToLimit();

  // Rebuild the output string from lines (truncated for UI)
  m_output = m_outputLines.join('\n');
  emit outputChanged();
}

void ProcessManager::onReadyReadStandardOutput() {
  appendOutput(QString::fromUtf8(m_process->readAllStandardOutput()));
}

void ProcessManager::onReadyReadStandardError() {
  appendOutput(QString::fromUtf8(m_process->readAllStandardError()));
}

void ProcessManager::killProcess() {
  if (m_terminalRunActive) {
    // The terminal reports the exit via terminalCommandFinished()
    signalTerminalProcessTree(SIGCONT);
    signalTerminalProcessTree(SIGKILL);
    emit terminalCloseRequested(m_terminalRunId);
    return;
  }

  if (m_process && m_process->state() != QProcess::NotRunning) {
    qint64 pid = m_process->processId();
    if (pid > 0) {
      // Send SIGKILL to the entire process group
      kill(-static_cast<pid_t>(pid), SIGKILL);
    }
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  setIsRunning(false);
}

QString ProcessManager::getHostname() { return QSysInfo::machineHostName(); }

bool ProcessManager::startDetached(const QString &program,
                                   const QStringList &arguments,
                                   const QString &workingDirectory) {
  return QProcess::startDetached(program, arguments, workingDirectory);
}

void ProcessManager::clearOutput() {
  m_output.clear();
  m_outputLines.clear();
  closeOutputLogFile();
  emit outputChanged();
  if (!m_terminalRunActive) {
    emit terminalResetRequested();
  }
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

void ProcessManager::setNotificationCommand(const QString &command) {
  m_notificationCommand = command;
}

void ProcessManager::setTerminalAvailable(bool available) {
  if (m_terminalAvailable != available) {
    m_terminalAvailable = available;
    emit terminalAvailableChanged();
  }
}

void ProcessManager::setTerminalMode(bool enabled) {
  if (m_terminalMode != enabled) {
    m_terminalMode = enabled;
    emit terminalModeChanged();
  }
}

QString ProcessManager::termEnvPrefix() const {
  // A real terminal supports colors and progress output, so only force a dumb
  // terminal when the output is captured as plain text
  return m_terminalMode ? QString() : QStringLiteral("env TERM=dumb ");
}

void ProcessManager::startTerminalCommand(const QString &program,
                                          const QStringList &arguments,
                                          const QString &workingDirectory) {
  QStringList quotedCommand{shellQuote(program)};
  for (const QString &argument : arguments) {
    quotedCommand.append(shellQuote(argument));
  }
  const QString shellCommand = "printf '%s\\n\\n' " +
                               shellQuote(m_terminalHeader) + "; exec " +
                               quotedCommand.join(' ');

  ++m_terminalRunId;
  m_terminalRunActive = true;
  m_terminalProcessId = 0;

  delete m_terminalTranscriptFile;
  m_terminalTranscriptFile = nullptr;

  const QString bash = QStandardPaths::findExecutable("bash");
  const QString env = QStandardPaths::findExecutable("env");
  const QString script = QStandardPaths::findExecutable("script");

  QString terminalProgram;
  QStringList terminalArguments;

  // Run the command through script(1), which keeps a full transcript for the
  // log file, because the terminal scrollback is limited
  if (!bash.isEmpty() && !env.isEmpty() && !script.isEmpty()) {
    auto *transcript = new QTemporaryFile(this);
    transcript->setAutoRemove(true);
    if (transcript->open()) {
      transcript->close();
      m_terminalTranscriptFile = transcript;
      terminalProgram = env;
      // script(1) runs the command with $SHELL, so make sure it is bash
      terminalArguments = {
          "SHELL=" + bash, script,      "--quiet",    "--flush",
          "--return",      "--command", shellCommand, transcript->fileName()};
    } else {
      delete transcript;
    }
  }

  if (terminalProgram.isEmpty()) {
    terminalProgram = bash.isEmpty() ? QStringLiteral("bash") : bash;
    terminalArguments = {"-c", shellCommand};
  }

  emit terminalCommandRequested(m_terminalRunId, terminalProgram,
                                terminalArguments, workingDirectory);
}

void ProcessManager::terminalCommandStarted(int runId, qint64 processId) {
  if (runId == m_terminalRunId && m_terminalRunActive) {
    m_terminalProcessId = processId;
  }
}

void ProcessManager::terminalCommandFinished(int runId, int exitCode,
                                             const QString &historyText,
                                             const QString &errorString) {
  if (runId != m_terminalRunId || !m_terminalRunActive) {
    return;
  }

  importTerminalTranscript(historyText);

  if (!errorString.isEmpty() && exitCode != 0) {
    appendOutput(QString("\nError: %1\n").arg(errorString));
  }

  handleProcessFinished(exitCode, false);
}

void ProcessManager::importTerminalTranscript(const QString &historyText) {
  if (!m_outputLogFile || !m_outputLogFile->isOpen()) {
    return;
  }

  QFile transcript(m_terminalTranscriptFile
                       ? m_terminalTranscriptFile->fileName()
                       : QString());
  if (m_terminalTranscriptFile && transcript.size() > 0 &&
      transcript.open(QIODevice::ReadOnly)) {
    bool firstLine = true;
    while (!transcript.atEnd()) {
      const QString rawLine = QString::fromUtf8(transcript.readLine());

      // Skip the start and end markers that script(1) always writes
      if ((firstLine && rawLine.startsWith("Script started on ")) ||
          rawLine.startsWith("Script done on ")) {
        firstLine = false;
        continue;
      }
      firstLine = false;

      const QString line = stripTerminalControls(rawLine);
      // Drop lines that only contained a redrawn and then erased status line
      QString content = rawLine;
      while (content.endsWith('\n') || content.endsWith('\r')) {
        content.chop(1);
      }
      if (line.isEmpty() && content.contains('\r')) {
        continue;
      }
      m_outputLogFile->write((line + '\n').toUtf8());
    }
  } else {
    m_outputLogFile->write(historyText.toUtf8());
  }

  delete m_terminalTranscriptFile;
  m_terminalTranscriptFile = nullptr;
}

void ProcessManager::signalTerminalProcessTree(int signal) {
  if (m_terminalProcessId <= 0) {
    return;
  }

  const pid_t root = static_cast<pid_t>(m_terminalProcessId);
  // Signal children first, so stopped parents cannot respawn anything
  QList<pid_t> pids = descendantProcessIds(root);
  std::reverse(pids.begin(), pids.end());
  pids.append(root);
  for (pid_t pid : pids) {
    // Processes owned by root (e.g. via sudo) fail with EPERM, like before
    kill(pid, signal);
  }
}

void ProcessManager::sendRebuildNotification(const QString &mode,
                                             const QString &hostname,
                                             int exitCode,
                                             qint64 elapsedMilliseconds) {
  if (m_notificationCommand.trimmed().isEmpty()) {
    return;
  }

  const QString result =
      exitCode == 0
          ? QString("Nixbit: %1 for %2 completed successfully in %3.")
                .arg(mode)
                .arg(hostname)
                .arg(formatDuration(elapsedMilliseconds))
          : QString("Nixbit: %1 for %2 failed with exit code %3 after %4.")
                .arg(mode)
                .arg(hostname)
                .arg(exitCode)
                .arg(formatDuration(elapsedMilliseconds));

  auto *notificationProcess = new QProcess(this);
  connect(notificationProcess,
          QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          notificationProcess, &QObject::deleteLater);
  connect(notificationProcess, &QProcess::errorOccurred, notificationProcess,
          [notificationProcess](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              qWarning() << "Failed to start notification command:"
                         << notificationProcess->errorString();
              notificationProcess->deleteLater();
            }
          });
  notificationProcess->start("sh", {"-c", m_notificationCommand});
  if (notificationProcess->waitForStarted(3000)) {
    notificationProcess->write((result + '\n').toUtf8());
    notificationProcess->closeWriteChannel();
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
  if (m_terminalRunActive) {
    signalTerminalProcessTree(SIGSTOP);
    setIsPaused(true);
    return;
  }
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
  if (m_terminalRunActive) {
    signalTerminalProcessTree(SIGCONT);
    setIsPaused(false);
    return;
  }
  qint64 pid = m_process->processId();
  if (pid > 0) {
    // Send SIGCONT to the entire process group
    kill(-static_cast<pid_t>(pid), SIGCONT);
    setIsPaused(false);
    appendOutput("\n\n=== Process resumed ===\n");
  }
}

void ProcessManager::runNixosRebuildBuild(const QString &repoPath,
                                          const QString &hostname,
                                          const QString &buildHost) {
  // Clear output before starting build
  clearOutput();

  // Sanitize hostname to prevent command injection
  QString sanitizedHostname = hostname;
  sanitizedHostname.replace(QRegularExpression("[^a-zA-Z0-9._-]"), "");

  if (sanitizedHostname.isEmpty() || sanitizedHostname != hostname) {
    appendOutput("Error: Invalid hostname. Only alphanumeric characters, "
                 "hyphens, underscores, and dots are allowed.\n");
    return;
  }

  // Sanitize build host if provided
  QString sanitizedBuildHost = buildHost.trimmed();
  if (!sanitizedBuildHost.isEmpty()) {
    sanitizedBuildHost.replace(QRegularExpression("[^a-zA-Z0-9._@-]"), "");
    if (sanitizedBuildHost.isEmpty() ||
        sanitizedBuildHost != buildHost.trimmed()) {
      appendOutput("Error: Invalid build host. Only alphanumeric characters, "
                   "hyphens, underscores, dots, and @ are allowed.\n");
      return;
    }
  }

  // Build the nixos-rebuild build command
  QString buildHostParam =
      sanitizedBuildHost.isEmpty() ? "" : " --build-host " + sanitizedBuildHost;
  QString cmd = "cd " + repoPath + " && " + termEnvPrefix() +
                "nixos-rebuild build --flake .#" + sanitizedHostname +
                buildHostParam + " -L";

  if (m_isRunning) {
    killProcess();
  }
  m_rebuildMode = "build";
  m_rebuildHostname = sanitizedHostname;
  m_rebuildTimer.start();
  runCommand("bash", QStringList() << "-c" << cmd);
}
void ProcessManager::runPrivilegedRebuild(const QString &mode,
                                          const QString &repoPath,
                                          const QString &hostname,
                                          const QString &buildHost) {
  // Clear output before starting the rebuild
  clearOutput();

  // Sanitize hostname to prevent command injection
  QString sanitizedHostname = hostname;
  sanitizedHostname.replace(QRegularExpression("[^a-zA-Z0-9._-]"), "");

  if (sanitizedHostname.isEmpty() || sanitizedHostname != hostname) {
    appendOutput("Error: Invalid hostname. Only alphanumeric characters, "
                 "hyphens, underscores, and dots are allowed.\n");
    return;
  }

  // Sanitize build host if provided
  QString sanitizedBuildHost = buildHost.trimmed();
  if (!sanitizedBuildHost.isEmpty()) {
    sanitizedBuildHost.replace(QRegularExpression("[^a-zA-Z0-9._@-]"), "");
    if (sanitizedBuildHost.isEmpty() ||
        sanitizedBuildHost != buildHost.trimmed()) {
      appendOutput("Error: Invalid build host. Only alphanumeric characters, "
                   "hyphens, underscores, dots, and @ are allowed.\n");
      return;
    }
  }

  // The rebuild mode is fully controlled by the application, but guard against
  // unexpected values reaching the elevated shell.
  if (mode != "switch" && mode != "boot") {
    appendOutput("Error: Invalid rebuild mode.\n");
    return;
  }

  QString buildHostParam =
      sanitizedBuildHost.isEmpty() ? "" : " --build-host " + sanitizedBuildHost;

  // Pre-flight: resolve the privilege escalation tool in C++ before running.
  // This avoids ever invoking a non-setuid pkexec (which would print a red
  // "pkexec must be setuid root" error into the log) and lets us report the
  // chosen tool with a neutral status line instead.
  EscalationTool tool = resolveEscalationTool();
  QString escalation;
  if (tool.found()) {
    appendOutput("\n=== Using privilege escalation: " + tool.description +
                 " ===\n\n");
    escalation = tool.command;
  } else {
    // No usable setuid tool found. Warn neutrally but still attempt with a
    // plain pkexec/sudo so the tool itself can report the precise problem.
    appendOutput(
        "\n=== No setuid pkexec or sudo wrapper was found; attempting anyway "
        "===\n"
        "=== If this fails, enable a setuid pkexec wrapper "
        "(security.wrappers.pkexec) or sudo on NixOS ===\n\n");
    escalation = "sudo";
  }

  // Build the script that will run as root. The repository is copied to a
  // private temporary directory (named with the elevated shell's PID) before
  // rebuilding, then cleaned up afterwards.
  QString scriptContent =
      "#!/usr/bin/env bash\n"
      "set -e\n"
      "TEMP_REPO=/tmp/nixbit-repo-$$\n"
      "echo \"Copying repository to temporary location...\"\n"
      "cp -r " +
      repoPath +
      " $TEMP_REPO\n"
      "cd $TEMP_REPO\n" +
      termEnvPrefix() + "nixos-rebuild " + mode + " --flake .#" +
      sanitizedHostname + buildHostParam +
      " -L\n"
      "echo \"Cleaning up temporary repository...\"\n"
      "rm -rf $TEMP_REPO\n";

  // Deliver the script to the elevated shell over stdin instead of writing it
  // to a predictable path in a world-writable directory. The script is
  // base64-encoded so its contents cannot break the surrounding command
  // regardless of any special characters in the repository path, and there is
  // no on-disk artifact that could be raced or replaced before running as root.
  QString encodedScript =
      QString::fromLatin1(scriptContent.toUtf8().toBase64());

  // The encoded payload is decoded inside the outer shell and piped into the
  // privileged shell (`<escalation> bash -s`). The escalation tool was resolved
  // above in C++, so the command is deterministic. stdin carries the script
  // source; the rebuild itself is non-interactive, and sudo authentication uses
  // an askpass helper (SUDO_ASKPASS), so stdin is free for this.
  QString askpassPrefix;
  if (!tool.askpass.isEmpty()) {
    // sudo -A needs SUDO_ASKPASS pointing at a graphical helper.
    askpassPrefix = "SUDO_ASKPASS=" + tool.askpass + " ";
  }

  QString cmd =
      "printf '%1' | base64 -d | (" + askpassPrefix + escalation + " bash -s)";

  if (m_isRunning) {
    killProcess();
  }
  m_rebuildMode = mode;
  m_rebuildHostname = sanitizedHostname;
  m_rebuildTimer.start();
  runCommand("bash", QStringList() << "-c" << cmd.arg(encodedScript));
}

void ProcessManager::runNixosRebuildSwitch(const QString &repoPath,
                                           const QString &hostname,
                                           const QString &buildHost) {
  runPrivilegedRebuild("switch", repoPath, hostname, buildHost);
}

void ProcessManager::runNixosRebuildBoot(const QString &repoPath,
                                         const QString &hostname,
                                         const QString &buildHost) {
  runPrivilegedRebuild("boot", repoPath, hostname, buildHost);
}

void ProcessManager::generateTestOutput(int lineCount) {
  if (m_isRunning) {
    qDebug() << "Cannot generate test output while process is running";
    return;
  }

  qDebug() << "Generating" << lineCount << "test lines...";

  // Show the plain text output view instead of a previous terminal session
  emit terminalResetRequested();

  // Sample log messages with various patterns that the highlighter will catch
  QStringList sampleLines = {
      "these 10 derivations will be built:",
      "  /nix/store/abc123-package-1.0.drv",
      "  /nix/store/def456-library-2.5.drv",
      "building '/nix/store/abc123-package-1.0.drv'...",
      "unpacking sources",
      "patching sources",
      "configuring",
      "building",
      "checking",
      "installing",
      "post-installation fixup",
      "shrinking RPATHs of ELF executables and libraries in /nix/store/xyz789",
      "gzipping man pages under /nix/store/xyz789/share/man/",
      "stripping (with command strip and flags -S -p) in /nix/store/xyz789/bin",
      "error: builder for '/nix/store/abc123-package-1.0.drv' failed with exit "
      "code 1",
      "warning: ignoring untrusted substituter 'https://example.com'",
      "trace: evaluating file '/path/to/file.nix'",
      "copying path '/nix/store/xyz789-result' from "
      "'https://cache.nixos.org'...",
      "fetching git repository 'https://github.com/user/repo.git'",
      "Created a new generation '42' at /nix/var/nix/profiles/system",
      "activating the configuration...",
      "setting up /etc...",
      "reloading user units for user...",
      "restarting the following units: display-manager.service",
      "the following new units were started: some-service.service",
      "info: Loaded 1234 packages from cache",
      "note: This is a test message with a note prefix",
      "failure: Something went wrong in the test",
      "success: Test operation completed successfully"};

  // Build all output as a single string to avoid triggering UI updates for each
  // line
  QString testOutput;
  testOutput.reserve(lineCount * 100); // Pre-allocate approximate memory

  testOutput += "=== Memory Test - Generating " + QString::number(lineCount) +
                " lines of output ===\n\n";

  // Generate the requested number of lines
  for (int i = 0; i < lineCount; ++i) {
    testOutput += QString("[%1] %2\n")
                      .arg(i + 1, 6, 10, QChar('0'))
                      .arg(sampleLines[i % sampleLines.size()]);

    // Add some build progress indicators every 100 lines
    if ((i + 1) % 100 == 0) {
      testOutput += QString("--- Progress: %1/%2 lines generated (%3%) ---\n")
                        .arg(i + 1)
                        .arg(lineCount)
                        .arg((i + 1) * 100 / lineCount);
    }
  }

  testOutput += "\n=== Test output generation complete ===\n";
  testOutput += QString("Total lines generated: %1\n").arg(lineCount);
  testOutput += QString("Approximate memory usage: %1 KB\n")
                    .arg(testOutput.length() / 1024);

  qDebug() << "Test output generated, length:" << testOutput.length()
           << "bytes";

  // Append all at once - this triggers only ONE UI update instead of thousands
  appendOutput(testOutput);

  m_hasFinished = true;
  emit hasFinishedChanged();
}
