#include "systemupdatestatus.h"
#include <QRegularExpression>

namespace {
bool isFullCommitHash(const QString &revision) {
  static const QRegularExpression commit(QStringLiteral("^[0-9a-f]{40}$"));
  return commit.match(revision).hasMatch();
}
} // namespace

SystemUpdateStatus::SystemUpdateStatus(QObject *parent,
                                       const QString &systemPath)
    : QObject(parent), m_systemPath(systemPath) {
  m_timeoutTimer.setSingleShot(true);
  m_timeoutTimer.setInterval(5000);
  connect(&m_timeoutTimer, &QTimer::timeout, this, [this]() {
    m_process.kill();
    setRunningRevision(QString());
  });
  connect(&m_process, &QProcess::errorOccurred, this,
          [this](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart) {
              m_timeoutTimer.stop();
              setRunningRevision(QString());
            }
          });
  connect(&m_process, &QProcess::finished, this,
          [this](int exitCode, QProcess::ExitStatus exitStatus) {
            m_timeoutTimer.stop();
            const QString revision =
                QString::fromUtf8(m_process.readAllStandardOutput()).trimmed();
            const bool ok = exitCode == 0 && exitStatus == QProcess::NormalExit;
            setRunningRevision(ok ? revision : QString());
          });

  // Also notice switches and reboots done outside of Nixbit. The running
  // system is used on purpose, the system profile may point to a generation
  // that was only made the boot default.
  m_refreshTimer.setInterval(30000);
  connect(&m_refreshTimer, &QTimer::timeout, this,
          &SystemUpdateStatus::refresh);
  m_refreshTimer.start();
  QTimer::singleShot(0, this, &SystemUpdateStatus::refresh);
}

SystemUpdateStatus::~SystemUpdateStatus() {
  if (m_process.state() != QProcess::NotRunning) {
    m_process.kill();
    m_process.waitForFinished(1000);
  }
}

void SystemUpdateStatus::refresh() {
  if (m_process.state() != QProcess::NotRunning) {
    return;
  }
  m_process.start(m_systemPath + QStringLiteral("/sw/bin/nixos-version"),
                  {QStringLiteral("--configuration-revision")});
  m_timeoutTimer.start();
}

void SystemUpdateStatus::setRunningRevision(const QString &revision) {
  if (m_runningRevision != revision) {
    m_runningRevision = revision;
    emit statusChanged();
  }
}

void SystemUpdateStatus::setLatestRevision(const QString &revision) {
  if (m_latestRevision != revision) {
    m_latestRevision = revision;
    emit statusChanged();
  }
}

SystemUpdateStatus::State
SystemUpdateStatus::compareRevisions(const QString &running,
                                     const QString &latest) {
  if (!isFullCommitHash(latest)) {
    return State::Unknown;
  }
  // A missing, dirty or abbreviated revision can't prove an exact match
  if (!isFullCommitHash(running)) {
    return State::Unverified;
  }
  return running == latest ? State::Current : State::NeedsActivation;
}

SystemUpdateStatus::State SystemUpdateStatus::state() const {
  return compareRevisions(m_runningRevision, m_latestRevision);
}

QString SystemUpdateStatus::runningRevisionShort() const {
  QString revision = m_runningRevision;
  const bool dirty = revision.endsWith(QStringLiteral("-dirty"));
  if (dirty) {
    revision.chop(6);
  }
  if (revision.isEmpty()) {
    return QString();
  }
  return revision.left(7) + (dirty ? QStringLiteral(" (dirty)") : QString());
}

QString SystemUpdateStatus::runningCommit() const {
  QString revision = m_runningRevision;
  if (revision.endsWith(QStringLiteral("-dirty"))) {
    revision.chop(6);
  }
  return isFullCommitHash(revision) ? revision : QString();
}

QString SystemUpdateStatus::summary() const {
  switch (state()) {
  case State::Current:
    return QStringLiteral("Up to date");
  case State::NeedsActivation:
    return QStringLiteral("Switch needed");
  case State::Unverified:
  case State::Unknown:
    break;
  }
  return QStringLiteral("Not verified");
}

QString SystemUpdateStatus::details() const {
  const QString running = runningRevisionShort();
  const QString latest = m_latestRevision.left(7);
  switch (state()) {
  case State::Current:
    return QStringLiteral("The running system was built from %1, the latest "
                          "fetched commit.")
        .arg(running);
  case State::NeedsActivation:
    return QStringLiteral(
               "The running system was built from %1, but the latest fetched "
               "commit is %2. Update the system in switch mode to activate "
               "it.")
        .arg(running, latest);
  case State::Unverified:
    if (m_runningRevision.endsWith(QStringLiteral("-dirty"))) {
      return QStringLiteral("The running system was built from %1 with "
                            "uncommitted changes, so it can't be compared "
                            "with the latest fetched commit %2.")
          .arg(m_runningRevision.left(7), latest);
    }
    return QStringLiteral(
        "The running system has no configuration revision, so it can't be "
        "compared with the latest fetched commit. Set "
        "system.configurationRevision in your flake to enable this check.");
  case State::Unknown:
    break;
  }
  return QStringLiteral(
      "The latest commit of the repository is not known yet.");
}
