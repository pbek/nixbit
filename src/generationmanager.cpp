#include "generationmanager.h"
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>

namespace {
const QString profilesPath = QStringLiteral("/nix/var/nix/profiles");

// Returns the commit hash of a configuration revision, without "-dirty"
QString commitOfRevision(const QString &revision) {
  static const QRegularExpression commit(
      QStringLiteral("^([0-9a-f]{40})(-dirty)?$"));
  const QRegularExpressionMatch match = commit.match(revision);
  return match.hasMatch() ? match.captured(1) : QString();
}
} // namespace

GenerationManager::GenerationManager(QObject *parent)
    : QAbstractListModel(parent), m_isLoading(false),
      m_currentGenerationNumber(0), m_bootGenerationNumber(0) {
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
  case IsBootDefaultRole:
    return gen.isBootDefault;
  case CommitRole:
    return commitOfRevision(gen.configurationRevision);
  case RevisionLabelRole: {
    const QString commit = commitOfRevision(gen.configurationRevision);
    if (commit.isEmpty()) {
      return QString();
    }
    const bool dirty =
        gen.configurationRevision.endsWith(QStringLiteral("-dirty"));
    return commit.left(7) + (dirty ? QStringLiteral(" (dirty)") : QString());
  }
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
  roles[IsBootDefaultRole] = "isBootDefault";
  roles[CommitRole] = "commit";
  roles[RevisionLabelRole] = "revisionLabel";
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
                           << profilesPath + "/");
}

void GenerationManager::readGenerationInfo(const QString &systemPath,
                                           Generation &generation) {
  // nixos-version is a script with the values embedded, reading it is much
  // cheaper than running it for every generation
  QFile script(systemPath + QStringLiteral("/sw/bin/nixos-version"));
  if (systemPath.isEmpty() || !script.open(QIODevice::ReadOnly)) {
    return;
  }
  const QString content = QString::fromUtf8(script.readAll());

  // Output of the --json option
  for (const QString &line : content.split('\n')) {
    if (!line.startsWith('{')) {
      continue;
    }
    const QJsonObject json = QJsonDocument::fromJson(line.toUtf8()).object();
    if (!json.isEmpty()) {
      generation.nixosVersion = json.value("nixosVersion").toString();
      generation.kernelVersion = json.value("kernelVersion").toString();
      generation.configurationRevision =
          json.value("configurationRevision").toString();
      return;
    }
  }

  // Older nixos-version scripts don't have the --json option
  static const QRegularExpression revisionRe(QStringLiteral(
      R"re(--configuration-revision\)(?:(?!;;)[\s\S])*?\n\s*echo "([0-9a-f]{40}(?:-dirty)?)")re"));
  const QRegularExpressionMatch match = revisionRe.match(content);
  if (match.hasMatch()) {
    generation.configurationRevision = match.captured(1);
  }
}

void GenerationManager::parseGenerations(const QString &output) {
  beginResetModel();
  m_generations.clear();

  QStringList lines = output.split('\n', Qt::SkipEmptyParts);

  // The system profile points to the generation that boots by default, which
  // isn't the running one after e.g. a boot rebuild
  static const QRegularExpression linkRe(R"(system-(\d+)-link)");
  const QRegularExpressionMatch bootMatch =
      linkRe.match(QFile::symLinkTarget(profilesPath + "/system"));
  const int bootGenNumber =
      bootMatch.hasMatch() ? bootMatch.captured(1).toInt() : 0;
  const QString runningPath =
      QFileInfo(QStringLiteral("/run/current-system")).canonicalFilePath();

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

      const QString systemPath =
          QFileInfo(
              QString("%1/system-%2-link").arg(profilesPath).arg(gen.number))
              .canonicalFilePath();
      gen.isCurrent = !runningPath.isEmpty() && systemPath == runningPath;
      gen.isBootDefault = gen.number == bootGenNumber;
      readGenerationInfo(systemPath, gen);

      m_generations.append(gen);
    }
  }

  // Sort generations by number in descending order (newest first)
  std::sort(m_generations.begin(), m_generations.end(),
            [](const Generation &a, const Generation &b) {
              return a.number > b.number;
            });

  // Several generations can point to the same system, e.g. after a rebuild
  // without changes. The list is sorted, so the newest one is used.
  int currentNumber = 0;
  QString currentDate;
  for (Generation &gen : m_generations) {
    if (gen.isCurrent && currentNumber == 0) {
      currentNumber = gen.number;
      currentDate = gen.dateTime;
    } else {
      gen.isCurrent = false;
    }
  }

  if (m_currentGenerationNumber != currentNumber ||
      m_currentGenerationDate != currentDate ||
      m_bootGenerationNumber != bootGenNumber) {
    m_currentGenerationNumber = currentNumber;
    m_currentGenerationDate = currentDate;
    m_bootGenerationNumber = bootGenNumber;
    emit currentGenerationChanged();
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
