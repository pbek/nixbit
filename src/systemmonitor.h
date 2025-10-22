#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QString>
#include <QTimer>

class SystemMonitor : public QObject {
  Q_OBJECT
  Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
  Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY memoryUsageChanged)
  Q_PROPERTY(QString networkStats READ networkStats NOTIFY networkStatsChanged)
  Q_PROPERTY(double systemLoad READ systemLoad NOTIFY systemLoadChanged)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
  explicit SystemMonitor(QObject *parent = nullptr);
  ~SystemMonitor();

  double cpuUsage() const { return m_cpuUsage; }
  double memoryUsage() const { return m_memoryUsage; }
  QString networkStats() const { return m_networkStats; }
  double systemLoad() const { return m_systemLoad; }
  bool active() const { return m_active; }

  void setActive(bool active);

signals:
  void cpuUsageChanged();
  void memoryUsageChanged();
  void networkStatsChanged();
  void systemLoadChanged();
  void activeChanged();

private slots:
  void updateStats();

private:
  void updateCpuUsage();
  void updateMemoryUsage();
  void updateNetworkStats();
  void updateSystemLoad();
  QString formatBytes(double bytes);

  QTimer *m_timer;
  double m_cpuUsage;
  double m_memoryUsage;
  QString m_networkStats;
  double m_systemLoad;
  bool m_active;

  // For CPU calculation
  unsigned long long m_prevTotal;
  unsigned long long m_prevIdle;

  // For network calculation
  unsigned long long m_prevRxBytes;
  unsigned long long m_prevTxBytes;
};

#endif // SYSTEMMONITOR_H
