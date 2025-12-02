#ifndef LOGMANAGER_H
#define LOGMANAGER_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QStringList>

struct LogEntry {
  QString fileName;
  QString filePath;
  QDateTime timestamp;
  QString displayName;
  int exitCode;
  QString buildType; // "build" or "switch"
  qint64 fileSize;   // Size in bytes
};

class LogManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QVariantList logFiles READ logFiles NOTIFY logFilesChanged)

public:
  explicit LogManager(QObject *parent = nullptr);

  QVariantList logFiles() const;

  Q_INVOKABLE void saveLog(const QString &output, int exitCode,
                           const QString &logDir, int maxLogs,
                           const QString &buildType);
  Q_INVOKABLE void openLogInEditor(const QString &filePath);
  Q_INVOKABLE void deleteLog(const QString &filePath);
  Q_INVOKABLE void refreshLogs(const QString &logDir);

signals:
  void logFilesChanged();

private:
  void cleanupOldLogs(const QString &logDir, int maxLogs);
  QList<LogEntry> parseLogFiles(const QString &logDir);

  QList<LogEntry> m_logEntries;
};

#endif // LOGMANAGER_H
