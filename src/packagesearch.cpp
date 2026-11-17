#include "packagesearch.h"
#include "utils/nix.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>

namespace {
// Maximum number of results shown
constexpr int maxResults = 200;
} // namespace

PackageSearch::PackageSearch(QObject *parent) : QObject(parent) {}

PackageSearch::~PackageSearch() {
  if (m_process) {
    m_process->disconnect(this);
    m_process->kill();
    m_process->waitForFinished(3000);
  }
  if (m_parseThread) {
    m_parseThread->disconnect(this);
    m_parseThread->wait();
    delete m_parseThread;
  }
}

void PackageSearch::setInstalledAttrNames(const QStringList &names) {
  const QSet<QString> installed(names.cbegin(), names.cend());
  if (installed == m_installed) {
    return;
  }
  m_installed = installed;
  updateResults();
}

void PackageSearch::setProgress(double progress, const QString &status) {
  m_progress = progress;
  m_status = status;
  emit indexProgressChanged();
}

void PackageSearch::loadIndex(bool force) {
  if (isIndexing() || (indexReady() && !force)) {
    return;
  }
  if (Utils::Nix::executable().isEmpty()) {
    m_error = QStringLiteral("Nix wasn't found in your PATH.");
    emit indexChanged();
    return;
  }

  m_error.clear();
  m_output.clear();
  m_errorLines.clear();
  m_partialLine.clear();
  m_cancelled = false;
  m_evaluating = false;
  setProgress(0, QStringLiteral("Loading the package index…"));

  auto *process = new QProcess(this);
  connect(process, &QProcess::readyReadStandardOutput, this,
          [this]() { m_output += m_process->readAllStandardOutput(); });
  connect(process, &QProcess::readyReadStandardError, this,
          &PackageSearch::onIndexOutput);
  connect(process, &QProcess::finished, this, &PackageSearch::onIndexFinished);
  connect(process, &QProcess::errorOccurred, this,
          [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart && process == m_process) {
              m_errorLines << "error: " + process->errorString();
              onIndexFinished(-1, QProcess::CrashExit);
            }
          });
  m_process = process;
  m_indexTimer.start();
  emit indexChanged();

  // "^" matches every package
  process->start(Utils::Nix::executable(),
                 Utils::Nix::baseArguments() +
                     QStringList{"search", "nixpkgs", "^", "--json"});

  Utils::Nix::run(this, {"eval", "--raw", "nixpkgs#lib.version"},
                  [this](const Utils::Nix::Result &result) {
                    // Only the release, e.g. "26.11" of
                    // "26.11.19700101.dirty" for a nixpkgs without Git info
                    static const QRegularExpression releaseRe(
                        QStringLiteral("^\\d+\\.\\d+"));
                    const QRegularExpressionMatch match = releaseRe.match(
                        QString::fromUtf8(result.standardOutput).trimmed());
                    if (result.ok() && match.hasMatch()) {
                      m_nixpkgsVersion = match.captured();
                      emit indexChanged();
                    }
                  });
}

void PackageSearch::cancelIndexing() {
  if (m_process) {
    m_cancelled = true;
    m_process->terminate();
  }
}

void PackageSearch::onIndexOutput() {
  if (!m_process) {
    return;
  }
  QString text =
      m_partialLine + QString::fromUtf8(m_process->readAllStandardError());
  text.replace('\r', '\n');
  QStringList lines = text.split('\n');
  m_partialLine = lines.takeLast();

  // nix reports every attribute it evaluates, which happens in alphabetical
  // order, so the first letters give a rough estimate of the progress
  static const QRegularExpression evaluatingRe(QStringLiteral(
      "^evaluating '(?:legacyPackages|packages)\\.[^.]+\\.([^.']+)"));
  for (const QString &rawLine : std::as_const(lines)) {
    const QString line = Utils::Nix::stripAnsi(rawLine).trimmed();
    const QRegularExpressionMatch match = evaluatingRe.match(line);
    if (!match.hasMatch()) {
      if (!line.isEmpty()) {
        m_errorLines << line;
        if (m_errorLines.size() > 50) {
          m_errorLines.removeFirst();
        }
      }
      continue;
    }

    const QString name = match.captured(1);
    double progress = 0.03;
    const QChar first = name.at(0).toLower();
    if (name.at(0).isLower() && first >= 'a' && first <= 'z') {
      double position = first.unicode() - 'a';
      if (name.size() > 1) {
        const QChar second = name.at(1).toLower();
        if (second >= 'a' && second <= 'z') {
          position += (second.unicode() - 'a') / 26.0;
        }
      }
      progress = 0.05 + 0.93 * position / 26.0;
    }
    // nix also reports the attributes it reads from the evaluation cache, which
    // is done after a few seconds
    m_evaluating = m_indexTimer.elapsed() > 3000;
    setProgress(std::max(m_progress, progress),
                QStringLiteral("Evaluating %1…").arg(name));
  }
}

void PackageSearch::onIndexFinished(int exitCode, QProcess::ExitStatus status) {
  QProcess *process = m_process;
  if (!process) {
    return;
  }
  m_output += process->readAllStandardOutput();
  m_process = nullptr;
  process->deleteLater();
  m_evaluating = false;

  if (m_cancelled) {
    m_output.clear();
    setProgress(0, QString());
    emit indexChanged();
    return;
  }

  if (status != QProcess::NormalExit || exitCode != 0) {
    Utils::Nix::Result result;
    result.started = true;
    result.exitCode = exitCode;
    result.standardError = m_errorLines.join('\n');
    m_error = result.errorMessage();
    m_output.clear();
    setProgress(0, QString());
    emit indexChanged();
    return;
  }

  // Parsing the JSON of more than 100,000 packages takes a moment, so it is
  // done in the background
  m_parsing = true;
  setProgress(1, QStringLiteral("Preparing the package index…"));
  auto holder = std::make_shared<std::shared_ptr<Entries>>();
  QThread *thread = QThread::create(
      [holder, json = std::move(m_output)]() { *holder = parseEntries(json); });
  m_output.clear();
  connect(thread, &QThread::finished, this, [this, thread, holder]() {
    m_parsing = false;
    m_entries = *holder;
    m_parseThread = nullptr;
    thread->deleteLater();
    if (!m_entries || m_entries->isEmpty()) {
      m_entries.reset();
      m_error = QStringLiteral("nix search returned no packages.");
    }
    setProgress(0, QString());
    emit indexChanged();
    updateResults();
  });
  m_parseThread = thread;
  emit indexChanged();
  thread->start();
}

std::shared_ptr<PackageSearch::Entries>
PackageSearch::parseEntries(const QByteArray &json) {
  auto entries = std::make_shared<Entries>();
  const QJsonObject object = QJsonDocument::fromJson(json).object();
  entries->reserve(object.size());

  for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
    const QJsonObject info = it.value().toObject();
    Entry entry;
    // "legacyPackages.x86_64-linux.python3Packages.rich" is installed as
    // "nixpkgs#python3Packages.rich"
    entry.attrName = it.key().section('.', 2);
    if (entry.attrName.isEmpty()) {
      continue;
    }
    entry.pname = info.value("pname").toString();
    entry.version = info.value("version").toString();
    entry.description = info.value("description").toString().simplified();
    entry.attrNameLower = entry.attrName.toLower();
    entry.lastNameLower = entry.attrNameLower.section('.', -1);
    entry.pnameLower = entry.pname.toLower();
    entry.descriptionLower = entry.description.toLower();
    entries->append(entry);
  }
  return entries;
}

void PackageSearch::search(const QString &query) {
  if (m_query == query) {
    return;
  }
  m_query = query;
  updateResults();
}

void PackageSearch::updateResults() {
  static const QRegularExpression whitespace(QStringLiteral("\\s+"));
  const QStringList tokens =
      m_query.toLower().split(whitespace, Qt::SkipEmptyParts);

  m_results.clear();
  m_matchCount = 0;
  if (tokens.isEmpty() || !m_entries) {
    emit resultsChanged();
    return;
  }

  const QString query = tokens.join(' ');
  struct Match {
    int score;
    const Entry *entry;
  };
  QVector<Match> matches;

  for (const Entry &entry : *m_entries) {
    bool allInName = true;
    bool matched = true;
    for (const QString &token : tokens) {
      const bool inName = entry.attrNameLower.contains(token) ||
                          entry.pnameLower.contains(token);
      if (!inName && !entry.descriptionLower.contains(token)) {
        matched = false;
        break;
      }
      allInName = allInName && inName;
    }
    if (!matched) {
      continue;
    }

    int score = 100;
    if (entry.attrNameLower == query) {
      score = 1000;
    } else if (entry.lastNameLower == query) {
      score = 920;
    } else if (entry.pnameLower == query) {
      score = 900;
    } else if (entry.attrNameLower.startsWith(query)) {
      score = 700;
    } else if (entry.lastNameLower.startsWith(query) ||
               entry.pnameLower.startsWith(query)) {
      score = 650;
    } else if (allInName) {
      score = 400;
    }
    // Prefer top-level packages over e.g. python3Packages.*
    if (entry.attrName.contains('.')) {
      score -= 60;
    }
    matches.append({score, &entry});
  }

  m_matchCount = matches.size();
  const qsizetype count = std::min<qsizetype>(matches.size(), maxResults);
  std::partial_sort(
      matches.begin(), matches.begin() + count, matches.end(),
      [](const Match &a, const Match &b) {
        if (a.score != b.score) {
          return a.score > b.score;
        }
        if (a.entry->attrName.size() != b.entry->attrName.size()) {
          return a.entry->attrName.size() < b.entry->attrName.size();
        }
        return a.entry->attrName < b.entry->attrName;
      });

  for (qsizetype i = 0; i < count; ++i) {
    const Entry *entry = matches.at(i).entry;
    QVariantMap result;
    result["attrName"] = entry->attrName;
    result["pname"] = entry->pname;
    result["version"] = entry->version;
    result["description"] = entry->description;
    result["installed"] = m_installed.contains(entry->attrName);
    m_results << result;
  }
  emit resultsChanged();
}
