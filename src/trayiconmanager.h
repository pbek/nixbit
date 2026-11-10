#ifndef TRAYICONMANAGER_H
#define TRAYICONMANAGER_H

#include "systemupdatestatus.h"
#include <QAction>
#include <QColor>
#include <QIcon>
#include <QMenu>
#include <QObject>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QVector>

class TrayIconManager : public QObject {
  Q_OBJECT

public:
  explicit TrayIconManager(bool debugMode = false, QObject *parent = nullptr);
  ~TrayIconManager();

  void setCommitsBehind(int count);
  // An empty activity restores the repository status icon.
  void setActivity(const QString &activity, bool animated = true);
  // The short revisions are shown in the tooltip, empty if unknown
  void setSystemStatus(SystemUpdateStatus::State state,
                       const QString &runningRevision = QString(),
                       const QString &latestRevision = QString());
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
  void updateIcon();
  enum class Badge { None, Check, Count, Busy, Pending };

  QIcon createDefaultIcon();
  QIcon createUpToDateIcon();
  QIcon createUpdateAvailableIcon(int count);
  QColor snowflakePrimaryColor() const;
  QColor snowflakeSecondaryColor() const;
  // Draws the Nix snowflake with an optional status badge in several sizes
  QIcon createStatusIcon(const QColor &primary, const QColor &secondary,
                         const QColor &badgeColor, Badge badge, int count = 0,
                         int angle = 0);

  QSystemTrayIcon *m_trayIcon;
  QMenu *m_trayMenu;
  QAction *m_showAction;
  QAction *m_checkUpdatesAction;
  QAction *m_ignoreUpdateAction;
  QAction *m_quitAction;
  int m_currentCommitsBehind;
  SystemUpdateStatus::State m_systemStatus = SystemUpdateStatus::State::Unknown;
  QString m_runningRevision;
  QString m_latestRevision;
  bool m_debugMode;
  QString m_activity;
  QTimer m_activityTimer;
  QVector<QIcon> m_activityFrames;
  int m_activityFrame = 0;
  // Id of the shown desktop notification, 0 if there is none
  uint m_notificationId;
};

#endif // TRAYICONMANAGER_H
