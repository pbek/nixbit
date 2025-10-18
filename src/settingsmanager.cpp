#include "settingsmanager.h"
#include <QDebug>

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

void SettingsManager::loadSettings() {
  QSettings settings("pbek", "nixbit");
  m_startHidden = settings.value("General/StartHidden", false).toBool();
  qDebug() << "Loaded start hidden setting:" << m_startHidden;
}

void SettingsManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("General/StartHidden", m_startHidden);
  settings.sync();
}
