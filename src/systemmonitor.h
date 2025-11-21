#ifndef SYSTEMMONITOR_H
#define SYSTEMMONITOR_H

#include <QObject>
#include <QString>
#include <QTimer>

class SystemMonitor : public QObject {
  Q_OBJECT
  Q_PROPERTY(double cpuUsage READ cpuUsage NOTIFY cpuUsageChanged)
  Q_PROPERTY(double memoryUsage READ memoryUsage NOTIFY memoryUsageChanged)
  Q_PROPERTY(QString totalMemory READ totalMemory NOTIFY totalMemoryChanged)
  Q_PROPERTY(QString usedMemory READ usedMemory NOTIFY usedMemoryChanged)
  Q_PROPERTY(QString networkStats READ networkStats NOTIFY networkStatsChanged)
  Q_PROPERTY(double systemLoad READ systemLoad NOTIFY systemLoadChanged)
  Q_PROPERTY(QString diskStats READ diskStats NOTIFY diskStatsChanged)
  Q_PROPERTY(bool active READ active WRITE setActive NOTIFY activeChanged)

public:
  explicit SystemMonitor(QObject *parent = nullptr);
  ~SystemMonitor();

  double cpuUsage() const { return m_cpuUsage; }
  double memoryUsage() const { return m_memoryUsage; }
  QString totalMemory() const { return m_totalMemory; }
  QString usedMemory() const { return m_usedMemory; }
  QString networkStats() const { return m_networkStats; }
  QString diskStats() const { return m_diskStats; }
  double systemLoad() const { return m_systemLoad; }
  bool active() const { return m_active; }

  void setActive(bool active);

signals:
  void cpuUsageChanged();
  void memoryUsageChanged();
  void totalMemoryChanged();
  void usedMemoryChanged();
  void diskStatsChanged();
  void networkStatsChanged();
  void systemLoadChanged();
  void activeChanged();

private slots:
  void updateStats();

private:
  void updateCpuUsage();
  void updateDiskStats();
  void updateMemoryUsage();
  void updateNetworkStats();
  void updateSystemLoad();
  QString formatBytes(double bytes);

  QTimer *m_timer;
  double m_cpuUsage;
  double m_memoryUsage;
  QString m_diskStats;
  QString m_totalMemory;
  QString m_usedMemory;
  QString m_networkStats;
  double m_systemLoad;
  bool m_active;

  // For CPU calculation
  unsigned long long m_prevTotal;

  // For disk I/O calculation
  unsigned long long m_prevReadBytes;
  unsigned long long m_prevWriteBytes;
  unsigned long long m_prevIdle;

  // For network calculation
  unsigned long long m_prevRxBytes;
  unsigned long long m_prevTxBytes;
};

#endif // SYSTEMMONITOR_H
