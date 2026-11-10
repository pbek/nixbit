#ifndef SYSTEMUPDATESTATUS_H
#define SYSTEMUPDATESTATUS_H

#include <QObject>
#include <QProcess>
#include <QTimer>

// Compares the configuration revision of the running NixOS system with the
// latest fetched commit of the configuration repository
class SystemUpdateStatus : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isUpToDate READ isUpToDate NOTIFY statusChanged)
  Q_PROPERTY(bool needsActivation READ needsActivation NOTIFY statusChanged)
  Q_PROPERTY(QString summary READ summary NOTIFY statusChanged)
  Q_PROPERTY(QString details READ details NOTIFY statusChanged)
  Q_PROPERTY(QString runningRevisionShort READ runningRevisionShort NOTIFY
                 statusChanged)
  // Full commit hash of the running system, without a "-dirty" suffix
  Q_PROPERTY(QString runningCommit READ runningCommit NOTIFY statusChanged)

public:
  enum class State { Unknown, Unverified, Current, NeedsActivation };

  explicit SystemUpdateStatus(
      QObject *parent = nullptr,
      const QString &systemPath = QStringLiteral("/run/current-system"));
  ~SystemUpdateStatus();

  State state() const;
  bool isUpToDate() const { return state() == State::Current; }
  bool needsActivation() const { return state() == State::NeedsActivation; }
  QString summary() const;
  QString details() const;
  QString runningRevisionShort() const;
  QString runningCommit() const;
  QString latestRevisionShort() const { return m_latestRevision.left(7); }
  void setLatestRevision(const QString &revision);

  static State compareRevisions(const QString &running, const QString &latest);

public slots:
  void refresh();

signals:
  void statusChanged();

private:
  void setRunningRevision(const QString &revision);

  QString m_systemPath;
  QString m_runningRevision;
  QString m_latestRevision;
  QProcess m_process;
  QTimer m_refreshTimer;
  QTimer m_timeoutTimer;
};

#endif // SYSTEMUPDATESTATUS_H
