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
  explicit TrayIconManager(QObject *parent = nullptr);
  ~TrayIconManager();

  void setCommitsBehind(int count);
  void show();
  void hide();

signals:
  void showWindowRequested();
  void checkForUpdatesRequested();
  void pullRepositoryRequested();
  void quitRequested();

private slots:
  void onActivated(QSystemTrayIcon::ActivationReason reason);

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
  QAction *m_pullAction;
  QAction *m_quitAction;
  int m_currentCommitsBehind;
};

#endif // TRAYICONMANAGER_H
