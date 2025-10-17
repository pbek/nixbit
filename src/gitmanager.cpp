#include "gitmanager.h"
#include <QDebug>
#include <QDir>
#include <QSettings>

// Credential callback for libgit2
static int credential_cb(git_credential **out, const char *url,
                         const char *username_from_url,
                         unsigned int allowed_types, void *payload) {
  Q_UNUSED(payload);

  qDebug() << "Credential callback invoked for URL:" << url;
  qDebug() << "Username from URL:"
           << (username_from_url ? username_from_url : "none");
  qDebug() << "Allowed credential types:" << allowed_types;

  // Try SSH agent first if SSH credentials are allowed
  if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
    qDebug() << "Trying SSH agent authentication...";
    int error = git_credential_ssh_key_from_agent(
        out, username_from_url ? username_from_url : "git");
    if (error == 0) {
      qDebug() << "SSH agent authentication successful";
      return 0;
    }
    qDebug() << "SSH agent authentication failed, error:" << error;
  }

  // Try default SSH key if available
  if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
    qDebug() << "Trying default SSH key...";
    QString homeDir = QDir::homePath();
    QString publicKey = homeDir + "/.ssh/id_rsa.pub";
    QString privateKey = homeDir + "/.ssh/id_rsa";

    if (QFile::exists(privateKey)) {
      int error = git_credential_ssh_key_new(
          out, username_from_url ? username_from_url : "git",
          publicKey.toUtf8().constData(), privateKey.toUtf8().constData(),
          "" // passphrase - empty for now
      );
      if (error == 0) {
        qDebug() << "SSH key authentication successful";
        return 0;
      }
      qDebug() << "SSH key authentication failed, error:" << error;
    }
  }

  // Use default credentials (for HTTPS without auth, or public repos)
  if (allowed_types & GIT_CREDENTIAL_DEFAULT) {
    qDebug() << "Using default credentials...";
    return git_credential_default_new(out);
  }

  // For username/password authentication
  if (allowed_types & GIT_CREDENTIAL_USERPASS_PLAINTEXT) {
    qDebug() << "Username/password auth requested but not implemented";
    // Future: Could prompt user for credentials here
  }

  qDebug() << "No suitable credential type available";
  return GIT_PASSTHROUGH;
}

// Progress callback for clone operations
static int transfer_progress_cb(const git_indexer_progress *stats,
                                void *payload) {
  GitManager *manager = static_cast<GitManager *>(payload);
  if (manager && stats->total_objects > 0) {
    int progress = (stats->received_objects * 100) / stats->total_objects;
    QString status = QString("Cloning: %1% (%2/%3 objects)")
                         .arg(progress)
                         .arg(stats->received_objects)
                         .arg(stats->total_objects);
    manager->setStatus(status);
  }
  return 0;
}

GitManager::GitManager(QObject *parent)
    : QObject(parent), m_repositoryUrl("https://github.com/pbek/nixcfg.git"),
      m_isBusy(false) {
  git_libgit2_init();

  loadSettings();

  // Set up the local path based on repository URL
  m_localPath = getRepositoryLocalPath(m_repositoryUrl);

  setStatus("Ready");
}

GitManager::~GitManager() {
  saveSettings();
  git_libgit2_shutdown();
}

void GitManager::setRepositoryUrl(const QString &url) {
  if (m_repositoryUrl != url) {
    m_repositoryUrl = url;
    m_localPath = getRepositoryLocalPath(url);
    saveSettings();
    emit repositoryUrlChanged();
    emit localPathChanged();
  }
}

void GitManager::setStatus(const QString &status) {
  if (m_status != status) {
    m_status = status;
    qDebug() << "GitManager status:" << status;
    emit statusChanged();
  }
}

void GitManager::setIsBusy(bool busy) {
  if (m_isBusy != busy) {
    m_isBusy = busy;
    emit isBusyChanged();
  }
}

void GitManager::loadSettings() {
  QSettings settings("pbek", "nixbit");

  QString savedUrl = settings.value("Repository/Url", QString()).toString();
  if (!savedUrl.isEmpty()) {
    m_repositoryUrl = savedUrl;
  }
}

void GitManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("Repository/Url", m_repositoryUrl);
  settings.sync();
}

QString GitManager::getRepositoryLocalPath(const QString &url) {
  // Extract repository name from URL
  QString repoName = url;
  repoName.replace(".git", "");

  int lastSlash = repoName.lastIndexOf('/');
  if (lastSlash >= 0) {
    repoName = repoName.mid(lastSlash + 1);
  }

  // Use Qt's data location
  QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return dataPath + "/repos/" + repoName;
}

void GitManager::cloneOrPullRepository() {
  if (m_isBusy) {
    qDebug() << "Already busy with an operation";
    return;
  }

  setIsBusy(true);

  // Check if repository already exists
  QDir repoDir(m_localPath);
  QDir gitDir(m_localPath + "/.git");

  bool success = false;
  QString message;

  if (gitDir.exists()) {
    setStatus("Pulling repository...");
    success = pullRepository_internal();
    message = success ? "Repository updated successfully"
                      : "Failed to pull repository";
  } else {
    setStatus("Cloning repository...");
    success = cloneRepository();
    message = success ? "Repository cloned successfully"
                      : "Failed to clone repository";
  }

  setIsBusy(false);
  emit operationCompleted(success, message);
}

void GitManager::pullRepository() {
  if (m_isBusy) {
    qDebug() << "Already busy with an operation";
    return;
  }

  setIsBusy(true);
  setStatus("Pulling repository...");

  bool success = pullRepository_internal();
  QString message =
      success ? "Repository updated successfully" : "Failed to pull repository";

  setIsBusy(false);
  emit operationCompleted(success, message);
}

bool GitManager::cloneRepository() {
  // Create parent directory if it doesn't exist
  QDir dir;
  QString parentPath = QFileInfo(m_localPath).absolutePath();
  if (!dir.mkpath(parentPath)) {
    setStatus("Failed to create directory");
    return false;
  }

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

  // Set up credential callback
  clone_opts.fetch_opts.callbacks.credentials = credential_cb;
  clone_opts.fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
  clone_opts.fetch_opts.callbacks.payload = this;

  // Set up callbacks for progress
  clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;

  int error = git_clone(&repo, m_repositoryUrl.toUtf8().constData(),
                        m_localPath.toUtf8().constData(), &clone_opts);

  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg =
        QString("Clone failed: %1").arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    return false;
  }

  git_repository_free(repo);
  setStatus("Clone completed successfully");
  return true;
}

bool GitManager::pullRepository_internal() {
  git_repository *repo = nullptr;

  // Open the repository
  int error = git_repository_open(&repo, m_localPath.toUtf8().constData());
  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg = QString("Failed to open repository: %1")
                           .arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    return false;
  }

  // Get the remote
  git_remote *remote = nullptr;
  error = git_remote_lookup(&remote, repo, "origin");
  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg = QString("Failed to lookup remote: %1")
                           .arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_repository_free(repo);
    return false;
  }

  // Fetch from remote with credential callback
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
  fetch_opts.callbacks.credentials = credential_cb;
  fetch_opts.callbacks.payload = this;

  error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);
  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg =
        QString("Fetch failed: %1").arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_remote_free(remote);
    git_repository_free(repo);
    return false;
  }

  // Try to find the default branch - check multiple possibilities
  git_oid target_oid;
  bool found_branch = false;

  const char *branch_names[] = {"refs/remotes/origin/main",
                                "refs/remotes/origin/master",
                                "refs/remotes/origin/HEAD"};

  for (const char *branch_name : branch_names) {
    error = git_reference_name_to_id(&target_oid, repo, branch_name);
    if (error == 0) {
      qDebug() << "Found branch:" << branch_name;
      found_branch = true;
      break;
    }
  }

  if (!found_branch) {
    const git_error *e = git_error_last();
    QString errorMsg = QString("Failed to find default branch: %1")
                           .arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_remote_free(remote);
    git_repository_free(repo);
    return false;
  }

  // Get the commit
  git_commit *target_commit = nullptr;
  error = git_commit_lookup(&target_commit, repo, &target_oid);
  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg = QString("Failed to lookup commit: %1")
                           .arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_remote_free(remote);
    git_repository_free(repo);
    return false;
  }

  // Reset to the commit
  error = git_reset(repo, (git_object *)target_commit, GIT_RESET_HARD, nullptr);
  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg =
        QString("Reset failed: %1").arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_commit_free(target_commit);
    git_remote_free(remote);
    git_repository_free(repo);
    return false;
  }

  // Cleanup
  git_commit_free(target_commit);
  git_remote_free(remote);
  git_repository_free(repo);

  setStatus("Pull completed successfully");
  return true;
}
