#include "systemresumedetector.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>

SystemResumeDetector::SystemResumeDetector(QObject *parent)
    : QObject(parent), m_loginInterface(nullptr), m_wasSleeping(false) {
  setupDBusConnection();
}

SystemResumeDetector::~SystemResumeDetector() {
  if (m_loginInterface) {
    delete m_loginInterface;
  }
}

void SystemResumeDetector::setupDBusConnection() {
  qDebug() << "Setting up system resume detection via D-Bus...";

  // Connect to systemd-logind's PrepareForSleep signal
  bool connected = QDBusConnection::systemBus().connect(
      "org.freedesktop.login1",         // service
      "/org/freedesktop/login1",        // path
      "org.freedesktop.login1.Manager", // interface
      "PrepareForSleep",                // signal name
      this,                             // receiver
      SLOT(onPrepareForSleep(bool))     // slot
  );

  if (connected) {
    qDebug()
        << "Successfully connected to systemd-logind PrepareForSleep signal";
  } else {
    qWarning() << "Failed to connect to systemd-logind PrepareForSleep signal";
    qWarning() << "System resume detection may not work properly";
  }
}

void SystemResumeDetector::onPrepareForSleep(bool sleeping) {
  if (sleeping) {
    qDebug() << "System is preparing to sleep";
    m_wasSleeping = true;
  } else {
    qDebug() << "System has resumed from sleep";
    if (m_wasSleeping) {
      m_wasSleeping = false;
      emit systemResumed();
    }
  }
}
