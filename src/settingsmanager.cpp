#include "settingsmanager.h"
#include <QCoreApplication>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QHostInfo>
#include <QStandardPaths>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent), m_startHidden(false), m_autostart(false) {
  loadSettings();
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

void SettingsManager::setAutostart(bool enabled) {
  if (m_autostart != enabled) {
    m_autostart = enabled;

    if (enabled) {
      if (createAutostartFile()) {
        qDebug() << "Autostart enabled successfully";
      } else {
        qDebug() << "Failed to enable autostart";
        m_autostart = false; // Revert if failed
      }
    } else {
      if (removeAutostartFile()) {
        qDebug() << "Autostart disabled successfully";
      } else {
        qDebug() << "Failed to disable autostart";
      }
    }

    saveSettings();
    emit autostartChanged();
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

  // Get the executable path
  QString execPath = QCoreApplication::applicationFilePath();

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

  // Check if autostart file exists and sync with settings
  bool fileExists = autostartFileExists();
  m_autostart = settings.value("General/Autostart", fileExists).toBool();

  // Ensure consistency between setting and actual file
  if (m_autostart && !fileExists) {
    createAutostartFile();
  } else if (!m_autostart && fileExists) {
    // User might have removed the setting but file still exists
    // Keep the file but update setting to match reality
    m_autostart = true;
    settings.setValue("General/Autostart", true);
  }

  qDebug() << "Loaded start hidden setting:" << m_startHidden;
  qDebug() << "Loaded hostname:" << m_hostname;
  qDebug() << "Loaded autostart setting:" << m_autostart;
}

void SettingsManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("General/StartHidden", m_startHidden);
  settings.setValue("General/Hostname", m_hostname);
  settings.setValue("General/Autostart", m_autostart);
  settings.sync();
}
