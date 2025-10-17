#ifndef GITMANAGER_H
#define GITMANAGER_H

#include <QObject>
#include <QSettings>
#include <QStandardPaths>
#include <QString>
#include <git2.h>

class GitManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(QString repositoryUrl READ repositoryUrl WRITE setRepositoryUrl
                 NOTIFY repositoryUrlChanged)
  Q_PROPERTY(QString localPath READ localPath NOTIFY localPathChanged)
  Q_PROPERTY(QString status READ status NOTIFY statusChanged)
  Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

public:
  explicit GitManager(QObject *parent = nullptr);
  ~GitManager();

  QString repositoryUrl() const { return m_repositoryUrl; }
  void setRepositoryUrl(const QString &url);

  QString localPath() const { return m_localPath; }
  QString status() const { return m_status; }
  bool isBusy() const { return m_isBusy; }

  Q_INVOKABLE void pullRepository();
  Q_INVOKABLE void cloneOrPullRepository();

  // Make setStatus public so callbacks can use it
  void setStatus(const QString &status);

signals:
  void repositoryUrlChanged();
  void localPathChanged();
  void statusChanged();
  void isBusyChanged();
  void operationCompleted(bool success, const QString &message);

private:
  void loadSettings();
  void saveSettings();
  void setIsBusy(bool busy);

  bool cloneRepository();
  bool pullRepository_internal();
  QString getRepositoryLocalPath(const QString &url);

  QString m_repositoryUrl;
  QString m_localPath;
  QString m_status;
  bool m_isBusy;
};

#endif // GITMANAGER_H
