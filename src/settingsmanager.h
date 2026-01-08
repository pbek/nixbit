#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QMap>
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
  Q_PROPERTY(QStringList buildHosts READ buildHosts NOTIFY buildHostsChanged)
  Q_PROPERTY(QString selectedBuildHost READ selectedBuildHost WRITE
                 setSelectedBuildHost NOTIFY selectedBuildHostChanged)
  Q_PROPERTY(QString selectedSwitchHost READ selectedSwitchHost WRITE
                 setSelectedSwitchHost NOTIFY selectedSwitchHostChanged)
  Q_PROPERTY(QString selectedBootHost READ selectedBootHost WRITE
                 setSelectedBootHost NOTIFY selectedBootHostChanged)
  Q_PROPERTY(int maxStoredLogs READ maxStoredLogs WRITE setMaxStoredLogs NOTIFY
                 maxStoredLogsChanged)
  Q_PROPERTY(
      bool debugMode READ debugMode WRITE setDebugMode NOTIFY debugModeChanged)

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

  QStringList buildHosts() const { return m_buildHosts; }
  Q_INVOKABLE void addBuildHost(const QString &name, const QString &address);
  Q_INVOKABLE void removeBuildHost(const QString &name);
  Q_INVOKABLE void updateBuildHost(const QString &oldName,
                                   const QString &newName,
                                   const QString &newAddress);
  Q_INVOKABLE QString getBuildHostAddress(const QString &name) const;

  QString selectedBuildHost() const { return m_selectedBuildHost; }
  void setSelectedBuildHost(const QString &host);

  QString selectedSwitchHost() const { return m_selectedSwitchHost; }
  void setSelectedSwitchHost(const QString &host);

  QString selectedBootHost() const { return m_selectedBootHost; }
  void setSelectedBootHost(const QString &host);

  int maxStoredLogs() const { return m_maxStoredLogs; }
  void setMaxStoredLogs(int count);

  bool debugMode() const { return m_debugMode; }
  void setDebugMode(bool enabled);

  Q_INVOKABLE QString getLogDirectory() const;

signals:
  void startHiddenChanged();
  void hostnameChanged();
  void autostartEnabledChanged();
  void windowWidthChanged();
  void windowHeightChanged();
  void windowXChanged();
  void windowYChanged();
  void buildHostChanged();
  void buildHostsChanged();
  void selectedBuildHostChanged();
  void selectedSwitchHostChanged();
  void selectedBootHostChanged();
  void maxStoredLogsChanged();
  void debugModeChanged();

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
  QStringList m_buildHosts;
  QMap<QString, QString> m_buildHostAddresses;
  QString m_selectedBuildHost;
  QString m_selectedSwitchHost;
  QString m_selectedBootHost;
  int m_maxStoredLogs;
  bool m_debugMode;
  int m_settingsVersion;
};

#endif // SETTINGSMANAGER_H
