#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>

class SettingsManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool startHidden READ startHidden WRITE setStartHidden NOTIFY
                 startHiddenChanged)
  Q_PROPERTY(
      QString hostname READ hostname WRITE setHostname NOTIFY hostnameChanged)
  Q_PROPERTY(bool autostartEnabled READ autostartEnabled NOTIFY
                 autostartEnabledChanged)

public:
  explicit SettingsManager(QObject *parent = nullptr);
  ~SettingsManager();

  bool startHidden() const { return m_startHidden; }
  void setStartHidden(bool hidden);

  QString hostname() const { return m_hostname; }
  void setHostname(const QString &hostname);

  bool autostartEnabled() const;
  Q_INVOKABLE void setAutostartEnabled(bool enabled);
  Q_INVOKABLE void checkAndCreateAutostart();

signals:
  void startHiddenChanged();
  void hostnameChanged();
  void autostartEnabledChanged();

private:
  void loadSettings();
  void saveSettings();
  QString getSystemHostname() const;
  QString getAutostartFilePath() const;
  bool autostartFileExists() const;
  bool createAutostartFile();
  bool removeAutostartFile();
  bool shouldForceAutostart() const;

  bool m_startHidden;
  QString m_hostname;
};

#endif // SETTINGSMANAGER_H
