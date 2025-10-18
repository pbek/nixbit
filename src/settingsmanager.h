#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>

class SettingsManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool startHidden READ startHidden WRITE setStartHidden NOTIFY
                 startHiddenChanged)

public:
  explicit SettingsManager(QObject *parent = nullptr);
  ~SettingsManager();

  bool startHidden() const { return m_startHidden; }
  void setStartHidden(bool hidden);

signals:
  void startHiddenChanged();

private:
  void loadSettings();
  void saveSettings();

  bool m_startHidden;
};

#endif // SETTINGSMANAGER_H
