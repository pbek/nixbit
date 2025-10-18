#ifndef GITMANAGER_H
#define GITMANAGER_H

#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <QTimer>
#include <git2.h>

class GitManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString repositoryUrl READ repositoryUrl WRITE setRepositoryUrl
                 NOTIFY repositoryUrlChanged)
  Q_PROPERTY(QString localPath READ localPath WRITE setLocalPath NOTIFY
                 localPathChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)
  Q_PROPERTY(int commitsBehind READ commitsBehind NOTIFY commitsBehindChanged)
  Q_PROPERTY(int fetchIntervalMinutes READ fetchIntervalMinutes WRITE
                 setFetchIntervalMinutes NOTIFY fetchIntervalMinutesChanged)

public:
  explicit GitManager(QObject *parent = nullptr);
  ~GitManager();

  QString repositoryUrl() const { return m_repositoryUrl; }
  void setRepositoryUrl(const QString &url);

  QString localPath() const { return m_localPath; }
  void setLocalPath(const QString &path);

  QString status() const { return m_status; }
  bool isBusy() const { return m_isBusy; }
  int commitsBehind() const { return m_commitsBehind; }
  int fetchIntervalMinutes() const { return m_fetchIntervalMinutes; }
  void setFetchIntervalMinutes(int minutes);

  Q_INVOKABLE void pullRepository();
  Q_INVOKABLE void cloneOrPullRepository();
  Q_INVOKABLE void checkForUpdates();

  // Make setStatus public so callbacks can use it
  void setStatus(const QString &status);

signals:
  void repositoryUrlChanged();
  void localPathChanged();
  void statusChanged();
  void isBusyChanged();
  void commitsBehindChanged();
  void fetchIntervalMinutesChanged();
  void operationCompleted(bool success, const QString &message);
  void pullCompletedForUpdate();

private slots:
  void onFetchTimerTimeout();

private:
  void loadSettings();
  void saveSettings();
  void setIsBusy(bool busy);
  void setCommitsBehind(int count);
  void startFetchTimer();
  void stopFetchTimer();

  bool cloneRepository();
  bool pullRepository_internal();
  bool fetchRepository();
  int calculateCommitsBehind();
  QString getRepositoryLocalPath(const QString &url);

  QString m_repositoryUrl;
  QString m_localPath;
  QString m_status;
  bool m_isBusy;
  int m_commitsBehind;
  int m_fetchIntervalMinutes;
  QTimer *m_fetchTimer;
};

#endif // GITMANAGER_H
