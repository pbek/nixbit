#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include <QAction>
#include <QMenu>
#include <QObject>
#include <QPixmap>
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
  QPixmap createDefaultIcon();
  QPixmap createUpToDateIcon();
  QPixmap createUpdateAvailableIcon(int count);

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
