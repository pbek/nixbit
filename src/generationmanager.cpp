#include "generationmanager.h"
#include <QDebug>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

GenerationManager::GenerationManager(QObject *parent)
    : QAbstractListModel(parent), m_isLoading(false),
      m_currentGenerationNumber(0) {
  m_refreshTimer = new QTimer(this);
  m_refreshTimer->setInterval(30000); // Refresh every 30 seconds
  connect(m_refreshTimer, &QTimer::timeout, this,
          &GenerationManager::loadGenerations);
}

int GenerationManager::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return m_generations.count();
}

QVariant GenerationManager::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() >= m_generations.count())
    return QVariant();

  const Generation &gen = m_generations.at(index.row());

  switch (role) {
  case NumberRole:
    return gen.number;
  case IsCurrentRole:
    return gen.isCurrent;
  case DateTimeRole:
    return gen.dateTime;
  case NixosVersionRole:
    return gen.nixosVersion;
  case KernelVersionRole:
    return gen.kernelVersion;
  case ConfigurationRevisionRole:
    return gen.configurationRevision;
  default:
    return QVariant();
  }
}

QHash<int, QByteArray> GenerationManager::roleNames() const {
  QHash<int, QByteArray> roles;
  roles[NumberRole] = "number";
  roles[IsCurrentRole] = "isCurrent";
  roles[DateTimeRole] = "dateTime";
  roles[NixosVersionRole] = "nixosVersion";
  roles[KernelVersionRole] = "kernelVersion";
  roles[ConfigurationRevisionRole] = "configurationRevision";
  return roles;
}

void GenerationManager::setActive(bool active) {
  if (active) {
    loadGenerations();
    m_refreshTimer->start();
  } else {
    m_refreshTimer->stop();
  }
}

void GenerationManager::loadGenerations() {
  m_isLoading = true;
  m_error.clear();
  emit isLoadingChanged();
  emit errorChanged();

  QProcess *process = new QProcess(this);

  connect(
      process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
      this, [this, process](int exitCode, QProcess::ExitStatus exitStatus) {
        QString output = QString::fromUtf8(process->readAllStandardOutput());
        QString errorOutput =
            QString::fromUtf8(process->readAllStandardError());

        if (exitStatus == QProcess::NormalExit && exitCode == 0) {
          parseGenerations(output);
        } else {
          m_error = "Failed to load generations: " + errorOutput;
          qWarning() << "Failed to load generations:" << errorOutput;
          emit errorChanged();
        }

        m_isLoading = false;
        emit isLoadingChanged();
        emit generationsLoaded();

        process->deleteLater();
      });

  // Use ls with full timestamp format to list system generation links
  process->start("ls", QStringList()
                           << "-l" << "--time-style=+%Y-%m-%d %H:%M:%S"
                           << "/nix/var/nix/profiles/");
}

void GenerationManager::parseGenerations(const QString &output) {
  beginResetModel();
  m_generations.clear();

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  // Get current generation by reading the system symlink
  QString currentLink = QFile::symLinkTarget("/nix/var/nix/profiles/system");
  QRegularExpression currentRe(R"(system-(\d+)-link)");
  QRegularExpressionMatch currentMatch = currentRe.match(currentLink);
  int currentGenNumber =
      currentMatch.hasMatch() ? currentMatch.captured(1).toInt() : 0;

  // Parse ls -l output
  // Format: "lrwxrwxrwx ... 2024-11-20 14:30:25 system-42-link ->
  // /nix/store/..."
  QRegularExpression re(
      R"(^l\S+\s+\d+\s+\S+\s+\S+\s+\d+\s+(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})\s+system-(\d+)-link)");

  for (const QString &line : lines) {
    QRegularExpressionMatch match = re.match(line);
    if (match.hasMatch()) {
      Generation gen;
      gen.number = match.captured(2).toInt();
      QString rawTimestamp = match.captured(1);
      gen.dateTime = formatDateTime(rawTimestamp);
      gen.isCurrent = (gen.number == currentGenNumber);

      gen.nixosVersion = "";
      gen.kernelVersion = "";
      gen.configurationRevision = "";

      m_generations.append(gen);
    }
  }

  // Sort generations by number in descending order (newest first)
  std::sort(m_generations.begin(), m_generations.end(),
            [](const Generation &a, const Generation &b) {
              return a.number > b.number;
            });

  // Update current generation info
  for (const Generation &gen : m_generations) {
    if (gen.isCurrent) {
      if (m_currentGenerationNumber != gen.number ||
          m_currentGenerationDate != gen.dateTime) {
        m_currentGenerationNumber = gen.number;
        m_currentGenerationDate = gen.dateTime;
        emit currentGenerationChanged();
      }
      break;
    }
  }

  endResetModel();
}

QString GenerationManager::formatDateTime(const QString &timestamp) {
  // Parse the timestamp and format it nicely
  QDateTime dt = QDateTime::fromString(timestamp, "yyyy-MM-dd hh:mm:ss");
  if (dt.isValid()) {
    // Return a more readable format
    QDateTime now = QDateTime::currentDateTime();
    qint64 daysAgo = dt.daysTo(now);

    if (daysAgo == 0) {
      return "Today at " + dt.toString("hh:mm");
    } else if (daysAgo == 1) {
      return "Yesterday at " + dt.toString("hh:mm");
    } else if (daysAgo < 7) {
      return QString("%1 days ago").arg(daysAgo);
    } else {
      return dt.toString("MMM dd, yyyy hh:mm");
    }
  }
  return timestamp;
}
