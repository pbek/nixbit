#include "processmanager.h"
#include <QDebug>

ProcessManager::ProcessManager(QObject *parent)
    : QObject(parent), m_process(nullptr), m_isRunning(false) {
  m_process = new QProcess(this);

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

void ProcessManager::killProcess() {
  if (m_process && m_process->state() != QProcess::NotRunning) {
    m_process->kill();
    m_process->waitForFinished(1000);
    appendOutput("\n\n=== Process killed ===\n");
  }
}

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
  m_output += text;
  emit outputChanged();
}

void ProcessManager::setIsRunning(bool running) {
  if (m_isRunning != running) {
    m_isRunning = running;
    emit isRunningChanged();
  }
}
