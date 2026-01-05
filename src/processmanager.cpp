#include "processmanager.h"
#include <QDateTime>
#include <QDebug>
#include <QRegularExpression>
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
  QString cmd = "cd " + repoPath +
                " && env TERM=dumb nixos-rebuild build --flake .#" +
                sanitizedHostname + buildHostParam + " -L";

  runCommand("bash", QStringList() << "-c" << cmd);
}

void ProcessManager::runNixosRebuildSwitch(const QString &repoPath,
                                           const QString &hostname,
                                           const QString &buildHost) {
  // Clear output before starting switch
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

  // Build the nixos-rebuild switch command with pkexec for sudo
  QString buildHostParam =
      sanitizedBuildHost.isEmpty() ? "" : " --build-host " + sanitizedBuildHost;
  QString timestamp = QString::number(QDateTime::currentMSecsSinceEpoch());
  QString tempScript = "/tmp/nixbit-rebuild-" + timestamp + ".sh";

  QString scriptContent =
      "#!/usr/bin/env bash\n"
      "set -e\n"
      "TEMP_REPO=/tmp/nixbit-repo-$$\n"
      "echo \"Copying repository to temporary location...\"\n"
      "cp -r " +
      repoPath +
      " $TEMP_REPO\n"
      "cd $TEMP_REPO\n"
      "env TERM=dumb nixos-rebuild switch --flake .#" +
      sanitizedHostname + buildHostParam +
      " -L\n"
      "echo \"Cleaning up temporary repository...\"\n"
      "rm -rf $TEMP_REPO\n";

  // Use a subshell to ensure cleanup happens but exit code is preserved
  QString cmd = "printf '%1' > " + tempScript + " && chmod +x " + tempScript +
                " && (pkexec " + tempScript + "; EXIT_CODE=$?; rm -f " +
                tempScript + "; exit $EXIT_CODE)";

  runCommand("bash", QStringList() << "-c" << cmd.arg(scriptContent));
}

void ProcessManager::generateTestOutput(int lineCount) {
  if (m_isRunning) {
    qDebug() << "Cannot generate test output while process is running";
    return;
  }

  qDebug() << "Generating" << lineCount << "test lines...";

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
