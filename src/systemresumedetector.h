#ifndef SYSTEMRESUMEDETECTOR_H
#define SYSTEMRESUMEDETECTOR_H

#include <QDBusConnection>
#include <QDBusInterface>
#include <QObject>

class SystemResumeDetector : public QObject {
  Q_OBJECT

public:
  explicit SystemResumeDetector(QObject *parent = nullptr);
  ~SystemResumeDetector();

signals:
  void systemResumed();

private slots:
  void onPrepareForSleep(bool sleeping);

private:
  void setupDBusConnection();

  QDBusInterface *m_loginInterface;
  bool m_wasSleeping;
};

#endif // SYSTEMRESUMEDETECTOR_H
