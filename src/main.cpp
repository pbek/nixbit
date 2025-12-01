#include "generationmanager.h"
#include "gitmanager.h"
#include "logmanager.h"
#include "outputhighlighter.h"
#include "processmanager.h"
#include "settingsmanager.h"
#include "systemmonitor.h"
#include "systemresumedetector.h"
#include "trayiconmanager.h"
#include "utils/cli.h"
#include "version.h"
#include <KLocalizedContext>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>

using namespace Qt::StringLiterals;

int main(int argc, char *argv[]) {
  // First, check if we're running in CLI-only mode (help, version, or
  // completion) Use QCoreApplication for these to avoid GUI initialization
  // overhead
  bool isCliOnlyMode = false;
  bool debugMode = false;
  for (int i = 1; i < argc; i++) {
    QString arg = QString::fromLocal8Bit(argv[i]);
    if (arg == "--help" || arg == "-h" || arg == "--version" || arg == "-v" ||
        arg == "--completion-bash" || arg == "--completion-fish") {
      isCliOnlyMode = true;
      break;
    }
    if (arg == "--debug") {
      debugMode = true;
    }
  }

  if (isCliOnlyMode) {
    // Use QCoreApplication for CLI-only operations
    QCoreApplication coreApp(argc, argv);
    QCoreApplication::setOrganizationName("pbek");
    QCoreApplication::setOrganizationDomain("pbek");
    QCoreApplication::setApplicationName("nixbit");
    QCoreApplication::setApplicationVersion(NIXBIT_VERSION);

    QCommandLineParser parser;
    parser.setApplicationDescription("A KDE Plasma application for updating "
                                     "NixOS systems from Git repositories.");

    QCommandLineOption helpOption = parser.addHelpOption();
    QCommandLineOption versionOption = parser.addVersionOption();

    QCommandLineOption completionBashOption(
        QStringList() << "completion-bash",
        "Generate Bash completion script and exit");
    parser.addOption(completionBashOption);

    QCommandLineOption completionFishOption(
        QStringList() << "completion-fish",
        "Generate Fish completion script and exit");
    parser.addOption(completionFishOption);

    QCommandLineOption debugOption(
        QStringList() << "debug",
        "Run in debug mode with separate settings and data directories");
    parser.addOption(debugOption);

    parser.process(coreApp);

    QList<QCommandLineOption> options;
    options << helpOption << versionOption << completionBashOption
            << completionFishOption << debugOption;

    if (parser.isSet(completionBashOption)) {
      Utils::Cli::generateBashCompletionScript(options, "nixbit");
      return 0;
    }

    if (parser.isSet(completionFishOption)) {
      Utils::Cli::generateFishCompletionScript(options, "nixbit");
      return 0;
    }

    // Help and version are automatically handled by parser.process()
    return 0;
  }

  // GUI mode - use QApplication
  QApplication app(argc, argv);

  QApplication::setOrganizationName("pbek");
  QApplication::setOrganizationDomain("pbek");
  QApplication::setApplicationVersion(NIXBIT_VERSION);

  // Use different organization/app name for debug mode to separate settings
  if (debugMode) {
    QApplication::setApplicationName("nixbit-debug");
    QApplication::setApplicationDisplayName("Nixbit (Debug) " +
                                            QStringLiteral(NIXBIT_VERSION));
  } else {
    QApplication::setApplicationName("nixbit");
    QApplication::setApplicationDisplayName("Nixbit " +
                                            QStringLiteral(NIXBIT_VERSION));
  }

  qDebug() << "Starting nixbit application...";

  KLocalizedString::setApplicationDomain("nixbit");
  QApplication::setWindowIcon(QIcon::fromTheme("nixbit"));

  QQmlApplicationEngine engine;

  // Make version and debug mode available to QML
  engine.rootContext()->setContextProperty("appVersion",
                                           QString(NIXBIT_VERSION));
  engine.rootContext()->setContextProperty("isDebugMode", debugMode);

  // Create and register GitManager
  GitManager gitManager;
  engine.rootContext()->setContextProperty("gitManager", &gitManager);

  // Create and register ProcessManager
  ProcessManager processManager;
  engine.rootContext()->setContextProperty("processManager", &processManager);

  // Create and register TrayIconManager
  TrayIconManager trayIconManager;
  engine.rootContext()->setContextProperty("trayIconManager", &trayIconManager);

  // Create and register SettingsManager
  SettingsManager settingsManager;
  engine.rootContext()->setContextProperty("settingsManager", &settingsManager);

  // Create and register SystemMonitor
  SystemMonitor systemMonitor;
  engine.rootContext()->setContextProperty("systemMonitor", &systemMonitor);

  // Create and register GenerationManager
  GenerationManager generationManager;
  engine.rootContext()->setContextProperty("generationManager",
                                           &generationManager);

  // Create and register OutputHighlighter
  OutputHighlighter outputHighlighter;
  engine.rootContext()->setContextProperty("outputHighlighter",
                                           &outputHighlighter);

  // Create and register LogManager
  LogManager logManager;
  engine.rootContext()->setContextProperty("logManager", &logManager);

  // Create SystemResumeDetector to check for updates after system resume
  SystemResumeDetector resumeDetector;
  QObject::connect(&resumeDetector, &SystemResumeDetector::systemResumed,
                   &gitManager, &GitManager::checkForUpdates);

  engine.rootContext()->setContextObject(new KLocalizedContext(&engine));

  qDebug() << "Loading QML from qrc:/qt/qml/nixbit/src/main.qml";

  // Load QML
  const QUrl url(u"qrc:/qt/qml/nixbit/src/main.qml"_s);

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

  // Toggle window visibility when tray icon is clicked
  QObject::connect(&trayIconManager, &TrayIconManager::toggleWindowRequested,
                   [mainWindow]() {
                     if (mainWindow) {
                       // If window is visible and active, hide it
                       if (mainWindow->isVisible() && mainWindow->isActive()) {
                         mainWindow->hide();
                       } else {
                         // Otherwise show and activate it
                         mainWindow->show();
                         mainWindow->raise();
                         mainWindow->requestActivate();
                       }
                     }
                   });

  QObject::connect(&trayIconManager, &TrayIconManager::checkForUpdatesRequested,
                   &gitManager, &GitManager::checkForUpdates);

  QObject::connect(&trayIconManager, &TrayIconManager::quitRequested, &app,
                   &QApplication::quit);

  // Update tray icon when commits behind changes
  QObject::connect(&gitManager, &GitManager::commitsBehindChanged,
                   [&gitManager, &trayIconManager]() {
                     trayIconManager.setCommitsBehind(
                         gitManager.commitsBehind());
                   });

  // Sync initial state
  trayIconManager.setCommitsBehind(gitManager.commitsBehind());

  // Show the tray icon
  trayIconManager.show();

  // Handle start hidden preference
  if (settingsManager.startHidden()) {
    qDebug() << "Starting hidden as per user preference";
    if (mainWindow) {
      mainWindow->hide();
    }
  }

  return app.exec();
}
