#include "gitmanager.h"
#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QDebug>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>

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
  return app.exec();
}
