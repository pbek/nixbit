#include "settingsmanager.h"
#include <QDebug>
#include <QHostInfo>

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent), m_startHidden(false) {
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

QString SettingsManager::getSystemHostname() const {
  return QHostInfo::localHostName();
}

void SettingsManager::loadSettings() {
  QSettings settings("pbek", "nixbit");
  m_startHidden = settings.value("General/StartHidden", false).toBool();
  m_hostname =
      settings.value("General/Hostname", getSystemHostname()).toString();
  qDebug() << "Loaded start hidden setting:" << m_startHidden;
  qDebug() << "Loaded hostname:" << m_hostname;
}

void SettingsManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("General/StartHidden", m_startHidden);
  settings.setValue("General/Hostname", m_hostname);
  settings.sync();
}
