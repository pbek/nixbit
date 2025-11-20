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
  Q_PROPERTY(int windowWidth READ windowWidth WRITE setWindowWidth NOTIFY
                 windowWidthChanged)
  Q_PROPERTY(int windowHeight READ windowHeight WRITE setWindowHeight NOTIFY
                 windowHeightChanged)
  Q_PROPERTY(int windowX READ windowX WRITE setWindowX NOTIFY windowXChanged)
  Q_PROPERTY(int windowY READ windowY WRITE setWindowY NOTIFY windowYChanged)
  Q_PROPERTY(QString buildHost READ buildHost WRITE setBuildHost NOTIFY
                 buildHostChanged)

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

  int windowWidth() const { return m_windowWidth; }
  void setWindowWidth(int width);

  int windowHeight() const { return m_windowHeight; }
  void setWindowHeight(int height);

  int windowX() const { return m_windowX; }
  void setWindowX(int x);

  int windowY() const { return m_windowY; }
  void setWindowY(int y);

  QString buildHost() const { return m_buildHost; }
  void setBuildHost(const QString &buildHost);

signals:
  void startHiddenChanged();
  void hostnameChanged();
  void autostartEnabledChanged();
  void windowWidthChanged();
  void windowHeightChanged();
  void windowXChanged();
  void windowYChanged();
  void buildHostChanged();

private:
  void loadSettings();
  void saveSettings();
  void migrateSettings(int fromVersion);
  QString getSystemHostname() const;
  QString getAutostartFilePath() const;
  bool autostartFileExists() const;
  bool createAutostartFile();
  bool removeAutostartFile();
  bool shouldForceAutostart() const;

  bool m_startHidden;
  QString m_hostname;
  int m_windowWidth;
  int m_windowHeight;
  int m_windowX;
  int m_windowY;
  QString m_buildHost;
  int m_settingsVersion;
};

#endif // SETTINGSMANAGER_H
