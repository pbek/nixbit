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
  Q_PROPERTY(
      bool autostart READ autostart WRITE setAutostart NOTIFY autostartChanged)

public:
  explicit SettingsManager(QObject *parent = nullptr);
  ~SettingsManager();

  bool startHidden() const { return m_startHidden; }
  void setStartHidden(bool hidden);

  QString hostname() const { return m_hostname; }
  void setHostname(const QString &hostname);

  bool autostart() const { return m_autostart; }
  void setAutostart(bool enabled);

signals:
  void startHiddenChanged();
  void hostnameChanged();
  void autostartChanged();

private:
  void loadSettings();
  void saveSettings();
  QString getSystemHostname() const;
  QString getAutostartFilePath() const;
  bool autostartFileExists() const;
  bool createAutostartFile();
  bool removeAutostartFile();

  bool m_startHidden;
  QString m_hostname;
  bool m_autostart;
};

#endif // SETTINGSMANAGER_H
