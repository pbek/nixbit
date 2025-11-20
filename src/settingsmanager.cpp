#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QStandardPaths>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent), m_startHidden(false), m_windowWidth(1000),
      m_windowHeight(800), m_windowX(-1), m_windowY(-1), m_buildHost("") {
  loadSettings();
  checkAndCreateAutostart();
}

SettingsManager::~SettingsManager() { saveSettings(); }

void SettingsManager::setStartHidden(bool hidden) {
  if (m_startHidden != hidden) {
    m_startHidden = hidden;
    saveSettings();
    emit startHiddenChanged();
    qDebug() << "Start hidden preference changed to:" << hidden;
  }
}

void SettingsManager::setHostname(const QString &hostname) {
  if (m_hostname != hostname) {
    m_hostname = hostname;
    saveSettings();
    emit hostnameChanged();
    qDebug() << "Hostname changed to:" << hostname;
  }
}

void SettingsManager::setWindowWidth(int width) {
  if (m_windowWidth != width) {
    m_windowWidth = width;
    saveSettings();
    emit windowWidthChanged();
  }
}

void SettingsManager::setWindowHeight(int height) {
  if (m_windowHeight != height) {
    m_windowHeight = height;
    saveSettings();
    emit windowHeightChanged();
  }
}

void SettingsManager::setWindowX(int x) {
  if (m_windowX != x) {
    m_windowX = x;
    saveSettings();
    emit windowXChanged();
  }
}

void SettingsManager::setWindowY(int y) {
  if (m_windowY != y) {
    m_windowY = y;
    saveSettings();
    emit windowYChanged();
  }
}

void SettingsManager::setBuildHost(const QString &buildHost) {
  if (m_buildHost != buildHost) {
    m_buildHost = buildHost;
    saveSettings();
    emit buildHostChanged();
    qDebug() << "Build host changed to:" << buildHost;
  }
}

bool SettingsManager::autostartEnabled() const { return autostartFileExists(); }

void SettingsManager::setAutostartEnabled(bool enabled) {
  bool currentState = autostartEnabled();

  if (currentState != enabled) {
    if (enabled) {
      if (createAutostartFile()) {
        qDebug() << "Autostart enabled successfully";
        emit autostartEnabledChanged();
      } else {
        qDebug() << "Failed to enable autostart";
      }
    } else {
      if (removeAutostartFile()) {
        qDebug() << "Autostart disabled successfully";
        emit autostartEnabledChanged();
      } else {
        qDebug() << "Failed to disable autostart";
      }
    }
  }
}

void SettingsManager::checkAndCreateAutostart() {
  if (shouldForceAutostart() && !autostartFileExists()) {
    qDebug() << "Force autostart is enabled, creating autostart file";
    if (createAutostartFile()) {
      emit autostartEnabledChanged();
    }
  }
}

QString SettingsManager::getSystemHostname() const {
  return QHostInfo::localHostName();
}

QString SettingsManager::getAutostartFilePath() const {
  QString configPath =
      QStandardPaths::writableLocation(QStandardPaths::ConfigLocation);
  return configPath + "/autostart/nixbit.desktop";
}

bool SettingsManager::autostartFileExists() const {
  return QFile::exists(getAutostartFilePath());
}

bool SettingsManager::shouldForceAutostart() const {
  // Check for system-wide override from /etc/nixbit.conf
  QSettings systemSettings("/etc/nixbit.conf", QSettings::IniFormat);
  bool forceAutostart = systemSettings.value("Autostart/Force", false).toBool();
  qDebug() << "Force autostart from global settings:" << forceAutostart;
  return forceAutostart;
}

bool SettingsManager::createAutostartFile() {
  QString autostartPath = getAutostartFilePath();
  QString autostartDir = QFileInfo(autostartPath).absolutePath();

  // Create autostart directory if it doesn't exist
  QDir dir;
  if (!dir.exists(autostartDir)) {
    if (!dir.mkpath(autostartDir)) {
      qDebug() << "Failed to create autostart directory:" << autostartDir;
      return false;
    }
  }

  // Set the executable
  QString execPath = QStringLiteral("nixbit");

  // Create the desktop file content
  QString desktopContent =
      QString("[Desktop Entry]\n"
              "Name=Nixbit\n"
              "GenericName=NixOS System Updater\n"
              "Comment=Update your NixOS system from a Git repository\n"
              "Exec=%1 --start-hidden\n"
              "Icon=nixbit\n"
              "Terminal=false\n"
              "Type=Application\n"
              "Categories=System;Settings;Qt;KDE;\n"
              "Keywords=nix;nixos;system;update;git;rebuild;\n"
              "StartupNotify=true\n"
              "StartupWMClass=nixbit\n"
              "X-KDE-StartupNotify=true\n"
              "X-DBUS-StartupType=Unique\n"
              "X-KDE-autostart-after=panel\n")
          .arg(execPath);

  // Write the file
  QFile file(autostartPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qDebug() << "Failed to open autostart file for writing:" << autostartPath;
    return false;
  }

  QTextStream out(&file);
  out << desktopContent;
  file.close();

  qDebug() << "Created autostart file:" << autostartPath;
  return true;
}

bool SettingsManager::removeAutostartFile() {
  QString autostartPath = getAutostartFilePath();

  if (!autostartFileExists()) {
    qDebug() << "Autostart file does not exist, nothing to remove";
    return true;
  }

  if (QFile::remove(autostartPath)) {
    qDebug() << "Removed autostart file:" << autostartPath;
    return true;
  } else {
    qDebug() << "Failed to remove autostart file:" << autostartPath;
    return false;
  }
}

void SettingsManager::loadSettings() {
  QSettings settings("pbek", "nixbit");
  m_startHidden = settings.value("General/StartHidden", false).toBool();
  m_hostname =
      settings.value("General/Hostname", getSystemHostname()).toString();
  m_windowWidth = settings.value("Window/Width", 1000).toInt();
  m_windowHeight = settings.value("Window/Height", 800).toInt();
  m_windowX = settings.value("Window/X", -1).toInt();
  m_windowY = settings.value("Window/Y", -1).toInt();
  m_buildHost = settings.value("General/BuildHost", "").toString();

  qDebug() << "Loaded start hidden setting:" << m_startHidden;
  qDebug() << "Loaded hostname:" << m_hostname;
  qDebug() << "Loaded window size:" << m_windowWidth << "x" << m_windowHeight;
  qDebug() << "Loaded window position:" << m_windowX << "," << m_windowY;
  qDebug() << "Loaded build host:" << m_buildHost;
  qDebug() << "Autostart file exists:" << autostartFileExists();
}

void SettingsManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("General/StartHidden", m_startHidden);
  settings.setValue("General/Hostname", m_hostname);
  settings.setValue("Window/Width", m_windowWidth);
  settings.setValue("Window/Height", m_windowHeight);
  settings.setValue("Window/X", m_windowX);
  settings.setValue("Window/Y", m_windowY);
  settings.setValue("General/BuildHost", m_buildHost);
  settings.sync();
}
