#include "gitmanager.h"
#include "processmanager.h"
#include "trayiconmanager.h"
#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  qDebug() << "Starting nixbit application...";

  KLocalizedString::setApplicationDomain("nixbit");

  QApplication::setOrganizationName("pbek");
  QApplication::setOrganizationDomain("pbek");
  QApplication::setApplicationName("nixbit");
  QApplication::setApplicationDisplayName("NixBit");
  QApplication::setWindowIcon(QIcon::fromTheme("git"));

  QQmlApplicationEngine engine;

  // Create and register GitManager
  GitManager gitManager;
  engine.rootContext()->setContextProperty("gitManager", &gitManager);

  // Create and register ProcessManager
  ProcessManager processManager;
  engine.rootContext()->setContextProperty("processManager", &processManager);

  // Create and register TrayIconManager
  TrayIconManager trayIconManager;
  engine.rootContext()->setContextProperty("trayIconManager", &trayIconManager);

  engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

  qDebug() << "Loading QML from qrc:/main.qml";

  // Load QML
  const QUrl url(QStringLiteral("qrc:/main.qml"));

  QObject::connect(
      &engine, &QQmlApplicationEngine::objectCreated, &app,
      [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl) {
          qCritical() << "ERROR: Failed to load QML file from" << url;
          QCoreApplication::exit(-1);
        }
      },
      Qt::QueuedConnection);

  engine.load(url);

  if (engine.rootObjects().isEmpty()) {
    qCritical() << "ERROR: Failed to load QML file - no root objects created";
    return -1;
  }

  qDebug() << "QML loaded successfully, starting application...";

  // Get the main window
  QObject *rootObject = engine.rootObjects().first();
  QQuickWindow *mainWindow = qobject_cast<QQuickWindow *>(rootObject);

  // Connect tray icon signals to window and git manager
  QObject::connect(&trayIconManager, &TrayIconManager::showWindowRequested,
                   [mainWindow]() {
                     if (mainWindow) {
                       mainWindow->show();
                       mainWindow->raise();
                       mainWindow->requestActivate();
                     }
                   });

  QObject::connect(&trayIconManager, &TrayIconManager::checkForUpdatesRequested,
                   &gitManager, &GitManager::checkForUpdates);

  QObject::connect(&trayIconManager, &TrayIconManager::pullRepositoryRequested,
                   &gitManager, &GitManager::pullRepository);

  QObject::connect(&trayIconManager, &TrayIconManager::quitRequested, &app,
                   &QApplication::quit);

  // Update tray icon when commits behind changes
  QObject::connect(&gitManager, &GitManager::commitsBehindChanged,
                   [&gitManager, &trayIconManager]() {
                     trayIconManager.setCommitsBehind(
                         gitManager.commitsBehind());
                   });

  // Show the tray icon
  trayIconManager.show();

  return app.exec();
}
