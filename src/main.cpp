#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QDebug>
#include <QProcess>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QStandardPaths>
#include <QUrl>

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  KLocalizedString::setApplicationDomain("nixbit");

  QCoreApplication::setOrganizationName(QStringLiteral("KDE"));
  QCoreApplication::setOrganizationDomain(QStringLiteral("kde.org"));
  QCoreApplication::setApplicationName(QStringLiteral("Nixbit"));
  QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

  QQmlApplicationEngine engine;

  engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

  // Try to load from install location first, then from source
  QString qmlFile = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                           QStringLiteral("nixbit/main.qml"));
  if (qmlFile.isEmpty()) {
    qmlFile = QStringLiteral("src/main.qml");
  }

  engine.load(QUrl::fromLocalFile(qmlFile));

  if (engine.rootObjects().isEmpty()) {
    qWarning() << "Failed to load QML file:" << qmlFile;
    return -1;
  }

  return app.exec();
}
