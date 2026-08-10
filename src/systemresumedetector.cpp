#include "systemresumedetector.h"
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
#include <QDebug>
#include <QNetworkInformation>
#include <QTimer>

SystemResumeDetector::SystemResumeDetector(QObject *parent)
    : QObject(parent), m_loginInterface(nullptr), m_networkInfo(nullptr),
      m_wasSleeping(false), m_waitingForNetwork(false) {
  setupDBusConnection();
  setupNetworkMonitoring();
  m_networkTimeoutTimer = new QTimer(this);
  connect(m_networkTimeoutTimer, &QTimer::timeout, this,
          &SystemResumeDetector::onNetworkTimeout);
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

void SystemResumeDetector::setupNetworkMonitoring() {
  qDebug() << "Setting up network monitoring...";

  if (!QNetworkInformation::loadDefaultBackend()) {
    qWarning() << "No QNetworkInformation backend is available; network "
                  "monitoring disabled";
    return;
  }

  m_networkInfo = QNetworkInformation::instance();
  if (m_networkInfo) {
    connect(m_networkInfo, &QNetworkInformation::reachabilityChanged, this,
            &SystemResumeDetector::onReachabilityChanged);
    qDebug() << "Network monitoring enabled";
  } else {
    qWarning()
        << "QNetworkInformation not available, network monitoring disabled";
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
      if (m_networkInfo &&
          m_networkInfo->reachability() !=
              QNetworkInformation::Reachability::Disconnected) {
        qDebug() << "Network is available, emitting systemResumed";
        emit systemResumed();
      } else {
        qDebug() << "Network not available, waiting for network...";
        m_waitingForNetwork = true;
        m_networkTimeoutTimer->start(10000); // 10 seconds timeout
      }
    }
  }
}

void SystemResumeDetector::onReachabilityChanged(
    QNetworkInformation::Reachability reachability) {
  qDebug() << "Network reachability changed:" << reachability;
  if (m_waitingForNetwork &&
      reachability != QNetworkInformation::Reachability::Disconnected) {
    qDebug() << "Network became available, emitting systemResumed";
    m_waitingForNetwork = false;
    m_networkTimeoutTimer->stop();
    emit systemResumed();
  }
}

void SystemResumeDetector::onNetworkTimeout() {
  qWarning() << "Network timeout occurred, stopping wait for network";
  m_waitingForNetwork = false;
  emit systemResumed();
}
