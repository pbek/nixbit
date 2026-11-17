#include "generationmanager.h"
#include "gitmanager.h"
#include "logmanager.h"
#include "outputhighlighter.h"
#include "packagemanager.h"
#include "packagesearch.h"
#include "processmanager.h"
#include "settingsmanager.h"
#include "systemmonitor.h"
#include "systemresumedetector.h"
#include "systemupdatestatus.h"
#include "trayiconmanager.h"
#include "utils/cli.h"
#include "version.h"
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <QApplication>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTimer>
#include <memory>

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

    parser.addOption(QCommandLineOption(
        "screenshot-scene", "Render an isolated demo scene", "scene"));
    parser.addOption(QCommandLineOption(
        "screenshot-output", "Save the demo window as PNG and exit", "path"));

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

  QCommandLineParser guiParser;
  guiParser.addOption(QCommandLineOption("debug"));
  guiParser.addOption(QCommandLineOption(
      "screenshot-scene", "Render an isolated demo scene", "scene"));
  guiParser.addOption(QCommandLineOption(
      "screenshot-output", "Save the demo window as PNG and exit", "path"));
  guiParser.process(app);

  // Do not even construct the real managers in demo mode: their constructors
  // read settings, create autostart entries and start background commands.
  if (guiParser.isSet("screenshot-scene")) {
    const QString scene = guiParser.value("screenshot-scene");
    const QStringList scenes = {"mainwindow",
                                "monitor",
                                "settings",
                                "settings-repository",
                                "settings-hosts",
                                "settings-notifications",
                                "settings-performance",
                                "settings-debug",
                                "about",
                                "commits",
                                "generations",
                                "logs",
                                "packages",
                                "packages-details",
                                "packages-search",
                                "packages-remove",
                                "packages-progress",
                                "packages-unfree",
                                "packages-conflict"};
    if (!scenes.contains(scene) ||
        guiParser.value("screenshot-output").isEmpty()) {
      qCritical() << "Use a supported screenshot scene and --screenshot-output";
      return 1;
    }
    KLocalization::setupLocalizedContext(&engine);
    QQmlComponent component(
        &engine, QUrl(u"qrc:/qt/qml/nixbit/src/ScreenshotData.qml"_s));
    std::unique_ptr<QObject> data(component.create());
    if (!data) {
      qCritical() << component.errors();
      return 1;
    }
    for (const auto &name :
         {"gitManager", "processManager", "settingsManager", "systemMonitor",
          "systemUpdateStatus", "generationManager", "packageManager",
          "packageSearch", "logManager"}) {
      engine.rootContext()->setContextProperty(
          QString::fromLatin1(name), data->property(name).value<QObject *>());
    }
    OutputHighlighter highlighter;
    engine.rootContext()->setContextProperty("outputHighlighter", &highlighter);
    engine.load(QUrl(u"qrc:/qt/qml/nixbit/src/main.qml"_s));
    if (engine.rootObjects().isEmpty())
      return 1;
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    if (!window)
      return 1;
    window->show();
    window->requestActivate();
    if (!QMetaObject::invokeMethod(window, "showScreenshotScene",
                                   Q_ARG(QVariant, scene))) {
      qCritical() << "Failed to open screenshot scene" << scene;
      return 1;
    }
    QTimer::singleShot(2500, &app, [&]() {
      const QImage image = window->grabWindow();
      app.exit(!image.isNull() &&
                       image.save(guiParser.value("screenshot-output"), "PNG")
                   ? 0
                   : 1);
    });
    return app.exec();
  }
  if (guiParser.isSet("screenshot-output")) {
    qCritical() << "--screenshot-output requires --screenshot-scene";
    return 1;
  }

  // Create and register GitManager
  GitManager gitManager;
  engine.rootContext()->setContextProperty("gitManager", &gitManager);

  // Create and register ProcessManager
  ProcessManager processManager;
  engine.rootContext()->setContextProperty("processManager", &processManager);

  // Create and register TrayIconManager
  TrayIconManager trayIconManager(debugMode);
  engine.rootContext()->setContextProperty("trayIconManager", &trayIconManager);

  SystemUpdateStatus systemUpdateStatus;
  engine.rootContext()->setContextProperty("systemUpdateStatus",
                                           &systemUpdateStatus);
  QObject::connect(&gitManager, &GitManager::latestFetchedRevisionChanged,
                   &systemUpdateStatus, [&]() {
                     systemUpdateStatus.setLatestRevision(
                         gitManager.latestFetchedRevision());
                   });
  QObject::connect(&systemUpdateStatus, &SystemUpdateStatus::statusChanged,
                   &trayIconManager, [&]() {
                     trayIconManager.setSystemStatus(
                         systemUpdateStatus.state(),
                         systemUpdateStatus.runningRevisionShort(),
                         systemUpdateStatus.latestRevisionShort());
                   });
  QObject::connect(&processManager, &ProcessManager::commandFinished,
                   &systemUpdateStatus, &SystemUpdateStatus::refresh);

  // Create and register SettingsManager
  SettingsManager settingsManager;
  engine.rootContext()->setContextProperty("settingsManager", &settingsManager);

  processManager.setNotificationCommand(settingsManager.notificationCommand());
  QObject::connect(&settingsManager,
                   &SettingsManager::notificationCommandChanged,
                   [&settingsManager, &processManager]() {
                     processManager.setNotificationCommand(
                         settingsManager.notificationCommand());
                   });

  // Create and register SystemMonitor
  SystemMonitor systemMonitor;
  engine.rootContext()->setContextProperty("systemMonitor", &systemMonitor);

  // Create and register GenerationManager
  GenerationManager generationManager;
  engine.rootContext()->setContextProperty("generationManager",
                                           &generationManager);
  // Notice switches done outside of Nixbit, e.g. with nixos-rebuild
  QObject::connect(&systemUpdateStatus, &SystemUpdateStatus::statusChanged,
                   &generationManager, &GenerationManager::loadGenerations);

  // Packages installed with "nix profile add", and the search for new ones
  PackageManager packageManager;
  engine.rootContext()->setContextProperty("packageManager", &packageManager);
  PackageSearch packageSearch;
  engine.rootContext()->setContextProperty("packageSearch", &packageSearch);
  QObject::connect(&packageManager, &PackageManager::packagesChanged,
                   &packageSearch, [&]() {
                     packageSearch.setInstalledAttrNames(
                         packageManager.installedAttrNames());
                   });
  QObject::connect(&packageManager, &PackageManager::installationsChanged,
                   &packageSearch, [&]() {
                     packageSearch.setInstallations(
                         packageManager.installations());
                   });
  packageManager.refresh();

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
  QObject::connect(&resumeDetector, &SystemResumeDetector::systemResumed,
                   &systemUpdateStatus, &SystemUpdateStatus::refresh);

  KLocalization::setupLocalizedContext(&engine);

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

  // Show the window with the new commits, so the user can decide whether to
  // update
  QObject::connect(
      &trayIconManager, &TrayIconManager::showCommitsRequested, [mainWindow]() {
        if (mainWindow) {
          mainWindow->show();
          mainWindow->raise();
          mainWindow->requestActivate();
          QMetaObject::invokeMethod(mainWindow, "showCommitsDialog");
        }
      });

  QObject::connect(&trayIconManager, &TrayIconManager::checkForUpdatesRequested,
                   &gitManager, &GitManager::checkForUpdates);

  QObject::connect(&trayIconManager, &TrayIconManager::ignoreUpdateRequested,
                   &gitManager, &GitManager::ignoreUpdate);

  QObject::connect(&trayIconManager, &TrayIconManager::quitRequested, &app,
                   &QApplication::quit);

  // Update tray icon when commits behind changes
  QObject::connect(
      &gitManager, &GitManager::commitsBehindChanged,
      [&gitManager, &trayIconManager]() {
        QStringList summaries;
        for (const QVariant &commit : gitManager.commitsBehindList()) {
          summaries << commit.toMap().value("message").toString();
        }
        trayIconManager.setCommitsBehind(gitManager.commitsBehind(), summaries);
      });

  // Tell the user when a command in the embedded terminal waits for a password
  // and they might not see it, because the window is hidden or not active
  QObject::connect(
      &processManager, &ProcessManager::waitingForPasswordChanged,
      [&processManager, &trayIconManager, mainWindow]() {
        // The prompt was answered or the process finished
        if (!processManager.waitingForPassword()) {
          trayIconManager.closeNotification();
          return;
        }
        if (mainWindow && mainWindow->isVisible() && mainWindow->isActive()) {
          return;
        }
        const QString hostname = processManager.rebuildHostname();
        trayIconManager.showNotification(
            "Password required",
            hostname.isEmpty()
                ? QString("Enter your password in Nixbit's terminal to "
                          "continue.")
                : QString("Enter your password in Nixbit's terminal to "
                          "continue the rebuild for %1.")
                      .arg(hostname));
      });

  // Sync initial state
  trayIconManager.setCommitsBehind(gitManager.commitsBehind());
  trayIconManager.setSystemStatus(systemUpdateStatus.state(),
                                  systemUpdateStatus.runningRevisionShort(),
                                  systemUpdateStatus.latestRevisionShort());

  // Combine activity sources so finishing one operation doesn't clear the
  // indicator while another is still running.
  const auto updateTrayActivity = [&]() {
    if (processManager.isRunning()) {
      if (processManager.waitingForPassword()) {
        trayIconManager.setActivity("Waiting for password", false);
      } else if (processManager.isPaused()) {
        trayIconManager.setActivity("Process paused", false);
      } else {
        trayIconManager.setActivity("Process running");
      }
    } else if (gitManager.isBusy()) {
      trayIconManager.setActivity("Updating repository");
    } else if (packageManager.isBusy()) {
      trayIconManager.setActivity(packageManager.activityText());
    } else {
      trayIconManager.setActivity(QString());
    }
  };
  QObject::connect(&gitManager, &GitManager::isBusyChanged, &trayIconManager,
                   updateTrayActivity);
  QObject::connect(&processManager, &ProcessManager::isRunningChanged,
                   &trayIconManager, updateTrayActivity);
  QObject::connect(&processManager, &ProcessManager::isPausedChanged,
                   &trayIconManager, updateTrayActivity);
  QObject::connect(&processManager, &ProcessManager::waitingForPasswordChanged,
                   &trayIconManager, updateTrayActivity);
  QObject::connect(&packageManager, &PackageManager::operationChanged,
                   &trayIconManager, updateTrayActivity);
  updateTrayActivity();

  // Show the tray icon
  trayIconManager.show();

  // Handle start hidden preference
  if (settingsManager.startHidden()) {
    qDebug() << "Starting hidden as per user preference";
    if (mainWindow) {
      mainWindow->hide();
    }
  } else {
    // Show window by default if not set to start hidden
    if (mainWindow) {
      mainWindow->show();
    }
  }

  return app.exec();
}
