#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include <QAction>
#include <QColor>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>

class TrayIconManager : public QObject {
  Q_OBJECT

public:
  explicit TrayIconManager(bool debugMode = false, QObject *parent = nullptr);
  ~TrayIconManager();

  void setCommitsBehind(int count);
  void show();
  void hide();
  // Shows a desktop notification with an action to show the window. A new
  // notification replaces the previous one.
  void showNotification(const QString &title, const QString &message);
  void closeNotification();

signals:
  void showWindowRequested();
  void toggleWindowRequested();
  void checkForUpdatesRequested();
  void ignoreUpdateRequested();
  void quitRequested();

private slots:
  void onActivated(QSystemTrayIcon::ActivationReason reason);
  void onNotificationActionInvoked(uint id, const QString &action);
  void onNotificationActivationToken(uint id, const QString &token);
  void onNotificationClosed(uint id, uint reason);

private:
  void createTrayIcon();
  void updateIcon(int commitsBehind);
  enum class Badge { None, Check, Count };

  QIcon createDefaultIcon();
  QIcon createUpToDateIcon();
  QIcon createUpdateAvailableIcon(int count);
  QColor snowflakePrimaryColor() const;
  QColor snowflakeSecondaryColor() const;
  // Draws the Nix snowflake with an optional status badge in several sizes
  QIcon createStatusIcon(const QColor &primary, const QColor &secondary,
                         const QColor &badgeColor, Badge badge, int count = 0);

  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  QAction *m_showAction;
  QAction *m_checkUpdatesAction;
  QAction *m_ignoreUpdateAction;
  QAction *m_quitAction;
  int m_currentCommitsBehind;
  bool m_debugMode;
  // Id of the shown desktop notification, 0 if there is none
  uint m_notificationId;
};

#endif // TRAYICONMANAGER_H
