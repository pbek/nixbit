#ifndef SYSTEMRESUMEDETECTOR_H
#define SYSTEMRESUMEDETECTOR_H

#include <QDBusConnection>
#include <QDBusInterface>
#include <QNetworkInformation>
#include <QObject>
#include <QTimer>

class SystemResumeDetector : public QObject {
  Q_OBJECT

public:
  explicit SystemResumeDetector(QObject *parent = nullptr);
  ~SystemResumeDetector();

signals:
  void systemResumed();

private slots:
  void onPrepareForSleep(bool sleeping);
  void onReachabilityChanged(QNetworkInformation::Reachability reachability);
  void onNetworkTimeout();

private:
  void setupDBusConnection();
  void setupNetworkMonitoring();

  QDBusInterface *m_loginInterface;
  QNetworkInformation *m_networkInfo;
  QTimer *m_networkTimeoutTimer;
  bool m_wasSleeping;
  bool m_waitingForNetwork;
};

#endif // SYSTEMRESUMEDETECTOR_H
