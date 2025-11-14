#include "systemmonitor.h"
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QTextStream>

SystemMonitor::SystemMonitor(QObject *parent)
    : QObject(parent), m_cpuUsage(0.0), m_memoryUsage(0.0), m_totalMemory(""),
      m_usedMemory(""), m_networkStats(""), m_systemLoad(0.0), m_active(false),
      m_prevTotal(0), m_prevIdle(0), m_prevRxBytes(0), m_prevTxBytes(0) {
  m_timer = new QTimer(this);
  connect(m_timer, &QTimer::timeout, this, &SystemMonitor::updateStats);
}

SystemMonitor::~SystemMonitor() { setActive(false); }

void SystemMonitor::setActive(bool active) {
  if (m_active == active)
    return;

  m_active = active;
  emit activeChanged();

  if (active) {
    // Initialize previous values
    updateCpuUsage();
    updateNetworkStats();
    updateMemoryUsage();
    updateSystemLoad();
    m_timer->start(2000); // Update every 2 seconds
  } else {
    m_timer->stop();
  }
}

void SystemMonitor::updateStats() {
  updateCpuUsage();
  updateMemoryUsage();
  updateNetworkStats();
  updateSystemLoad();
}

void SystemMonitor::updateCpuUsage() {
  QFile file("/proc/stat");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  QString line = in.readLine();
  if (!line.startsWith("cpu "))
    return;

  QStringList parts =
      line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  if (parts.size() < 8)
    return;

  unsigned long long user = parts[1].toULongLong();
  unsigned long long nice = parts[2].toULongLong();
  unsigned long long system = parts[3].toULongLong();
  unsigned long long idle = parts[4].toULongLong();
  unsigned long long iowait = parts[5].toULongLong();
  unsigned long long irq = parts[6].toULongLong();
  unsigned long long softirq = parts[7].toULongLong();

  unsigned long long total =
      user + nice + system + idle + iowait + irq + softirq;

  if (m_prevTotal > 0) {
    unsigned long long totalDiff = total - m_prevTotal;
    unsigned long long idleDiff = idle - m_prevIdle;

    if (totalDiff > 0) {
      double usage = 100.0 * (totalDiff - idleDiff) / totalDiff;
      if (usage != m_cpuUsage) {
        m_cpuUsage = usage;
        emit cpuUsageChanged();
      }
    }
  }

  m_prevTotal = total;
  m_prevIdle = idle;
}

void SystemMonitor::updateMemoryUsage() {
  QFile file("/proc/meminfo");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  unsigned long long totalMem = 0, availableMem = 0;

  // Read entire file content at once
  QByteArray content = file.readAll();
  file.close();

  // Split into lines
  QList<QByteArray> lines = content.split('\n');

  for (const QByteArray &lineData : lines) {
    QString line = QString::fromLatin1(lineData);

    if (line.startsWith("MemTotal:")) {
      // Extract number from "MemTotal:       65747368 kB"
      QStringList parts =
          line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      if (parts.size() >= 2) {
        totalMem = parts[1].toULongLong() * 1024; // Convert kB to bytes
      }
    } else if (line.startsWith("MemAvailable:")) {
      // Extract number from "MemAvailable:   49541176 kB"
      QStringList parts =
          line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
      if (parts.size() >= 2) {
        availableMem = parts[1].toULongLong() * 1024; // Convert kB to bytes
      }
    }

    // Stop early if we have both values
    if (totalMem > 0 && availableMem > 0)
      break;
  }

  if (totalMem > 0) {
    unsigned long long usedMem = totalMem - availableMem;
    double usage = 100.0 * usedMem / totalMem;

    if (usage != m_memoryUsage) {
      m_memoryUsage = usage;
      emit memoryUsageChanged();
    }

    QString newTotal = formatBytes(totalMem);
    if (newTotal != m_totalMemory) {
      m_totalMemory = newTotal;
      emit totalMemoryChanged();
    }

    QString newUsed = formatBytes(usedMem);
    if (newUsed != m_usedMemory) {
      m_usedMemory = newUsed;
      emit usedMemoryChanged();
    }
  }
}

void SystemMonitor::updateNetworkStats() {
  QFile file("/proc/net/dev");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  // Skip header lines
  in.readLine();
  in.readLine();

  unsigned long long rxBytes = 0, txBytes = 0;

  while (!in.atEnd()) {
    QString line = in.readLine();
    QStringList parts =
        line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() >= 10) {
      // Skip loopback interface
      if (parts[0].endsWith(":lo"))
        continue;

      rxBytes += parts[1].toULongLong();
      txBytes += parts[9].toULongLong();
    }
  }

  // Calculate rates (bytes per second)
  static qint64 lastUpdate = 0;
  qint64 now = QDateTime::currentMSecsSinceEpoch();
  double timeDiff = (now - lastUpdate) / 1000.0;

  if (lastUpdate > 0 && timeDiff > 0) {
    double rxRate = (rxBytes - m_prevRxBytes) / timeDiff;
    double txRate = (txBytes - m_prevTxBytes) / timeDiff;

    QString stats =
        QString("↓ %1/s ↑ %2/s").arg(formatBytes(rxRate), formatBytes(txRate));
    if (stats != m_networkStats) {
      m_networkStats = stats;
      emit networkStatsChanged();
    }
  }

  m_prevRxBytes = rxBytes;
  m_prevTxBytes = txBytes;
  lastUpdate = now;
}

void SystemMonitor::updateSystemLoad() {
  QFile file("/proc/loadavg");
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream in(&file);
  QString line = in.readLine();
  QStringList parts =
      line.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
  if (parts.size() >= 1) {
    double load = parts[0].toDouble();
    if (load != m_systemLoad) {
      m_systemLoad = load;
      emit systemLoadChanged();
    }
  }
}

QString SystemMonitor::formatBytes(double bytes) {
  const QStringList units = {"B", "KB", "MB", "GB"};
  int unitIndex = 0;
  while (bytes >= 1024 && unitIndex < units.size() - 1) {
    bytes /= 1024;
    unitIndex++;
  }
  return QString("%1%2").arg(bytes, 0, 'f', 1).arg(units[unitIndex]);
}
