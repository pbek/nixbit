#include "logmanager.h"
#include <QDebug>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QUrl>

LogManager::LogManager(QObject *parent) : QObject(parent) {}

QVariantList LogManager::logFiles() const {
  QVariantList result;
  for (const LogEntry &entry : m_logEntries) {
    QVariantMap map;
    map["fileName"] = entry.fileName;
    map["filePath"] = entry.filePath;
    map["timestamp"] = entry.timestamp;
    map["displayName"] = entry.displayName;
    map["exitCode"] = entry.exitCode;
    result.append(map);
  }
  return result;
}

void LogManager::saveLog(const QString &output, int exitCode,
                         const QString &logDir, int maxLogs) {
  if (output.isEmpty()) {
    qDebug() << "Not saving empty log";
    return;
  }

  // Create log directory if it doesn't exist
  QDir dir(logDir);
  if (!dir.exists()) {
    if (!dir.mkpath(".")) {
      qDebug() << "Failed to create log directory:" << logDir;
      return;
    }
  }

  // Generate filename with timestamp
  QString timestamp =
      QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss");
  QString fileName =
      QString("build_%1_exit%2.log").arg(timestamp).arg(exitCode);
  QString filePath = logDir + "/" + fileName;

  // Write log file
  QFile file(filePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug() << "Failed to open log file for writing:" << filePath;
    return;
  }

  QTextStream out(&file);
  out << output;
  file.close();

  qDebug() << "Saved build log to:" << filePath;

  // Cleanup old logs
  cleanupOldLogs(logDir, maxLogs);

  // Refresh the log list
  refreshLogs(logDir);
}

void LogManager::openLogInEditor(const QString &filePath) {
  qDebug() << "Opening log in editor:" << filePath;

  // Try to use xdg-open (works on most Linux systems)
  QProcess::startDetached("xdg-open", QStringList() << filePath);
}

void LogManager::deleteLog(const QString &filePath) {
  QFile file(filePath);
  if (file.exists()) {
    if (file.remove()) {
      qDebug() << "Deleted log file:" << filePath;

      // Refresh the log list
      QFileInfo fileInfo(filePath);
      refreshLogs(fileInfo.absolutePath());
    } else {
      qDebug() << "Failed to delete log file:" << filePath;
    }
  }
}

void LogManager::refreshLogs(const QString &logDir) {
  m_logEntries = parseLogFiles(logDir);
  emit logFilesChanged();
}

void LogManager::cleanupOldLogs(const QString &logDir, int maxLogs) {
  if (maxLogs <= 0) {
    return; // No limit
  }

  QList<LogEntry> logs = parseLogFiles(logDir);

  // If we have more logs than the limit, delete the oldest ones
  if (logs.size() > maxLogs) {
    int logsToDelete = logs.size() - maxLogs;
    qDebug() << "Cleaning up" << logsToDelete << "old log(s)";

    for (int i = 0; i < logsToDelete; ++i) {
      QFile::remove(logs[i].filePath);
      qDebug() << "Deleted old log:" << logs[i].filePath;
    }
  }
}

QList<LogEntry> LogManager::parseLogFiles(const QString &logDir) {
  QList<LogEntry> entries;

  QDir dir(logDir);
  if (!dir.exists()) {
    return entries;
  }

  // Get all .log files
  QStringList filters;
  filters << "build_*.log";
  QFileInfoList fileList =
      dir.entryInfoList(filters, QDir::Files, QDir::Time | QDir::Reversed);

  // Parse each log file
  QRegularExpression filenamePattern(
      R"(build_(\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2})_exit(\d+)\.log)");

  for (const QFileInfo &fileInfo : fileList) {
    LogEntry entry;
    entry.fileName = fileInfo.fileName();
    entry.filePath = fileInfo.absoluteFilePath();

    QRegularExpressionMatch match = filenamePattern.match(entry.fileName);
    if (match.hasMatch()) {
      QString timestampStr = match.captured(1);
      entry.timestamp =
          QDateTime::fromString(timestampStr, "yyyy-MM-dd_HH-mm-ss");
      entry.exitCode = match.captured(2).toInt();

      // Create display name
      QString exitStatus =
          entry.exitCode == 0 ? "Success"
                              : QString("Failed (exit %1)").arg(entry.exitCode);
      entry.displayName =
          QString("%1 - %2")
              .arg(entry.timestamp.toString("yyyy-MM-dd HH:mm:ss"))
              .arg(exitStatus);
    } else {
      // Fallback for files that don't match the pattern
      entry.timestamp = fileInfo.lastModified();
      entry.exitCode = -1;
      entry.displayName = entry.fileName;
    }

    entries.append(entry);
  }

  // Sort by timestamp, newest first
  std::sort(entries.begin(), entries.end(),
            [](const LogEntry &a, const LogEntry &b) {
              return a.timestamp > b.timestamp;
            });

  return entries;
}
