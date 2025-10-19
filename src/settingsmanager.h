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

public:
  explicit SettingsManager(QObject *parent = nullptr);
  ~SettingsManager();

  bool startHidden() const { return m_startHidden; }
  void setStartHidden(bool hidden);

  QString hostname() const { return m_hostname; }
  void setHostname(const QString &hostname);

signals:
  void startHiddenChanged();
  void hostnameChanged();

private:
  void loadSettings();
  void saveSettings();
  QString getSystemHostname() const;

  bool m_startHidden;
  QString m_hostname;
};

#endif // SETTINGSMANAGER_H
