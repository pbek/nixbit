#include "gitmanager.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QSettings>

// Structure to track credential attempts
struct CredentialData {
  int ssh_agent_tried = 0;
  int ssh_key_tried = 0;
  GitManager *manager = nullptr;
  QString passphrase; // Store passphrase if needed
};

// Certificate callback for SSH host key verification
static int certificate_check_cb(git_cert *cert, int valid, const char *host,
                                void *payload) {
  Q_UNUSED(payload);

  qDebug() << "Certificate check callback invoked for host:" << host;
  qDebug() << "Certificate valid:" << valid;
  qDebug() << "Certificate type:" << cert->cert_type;

  // For SSH connections, we need to accept the host key
  // In production, you might want to verify against known_hosts
  if (cert->cert_type == GIT_CERT_HOSTKEY_LIBSSH2) {
    qDebug() << "Accepting SSH host key for" << host;
    return 0; // Accept the certificate
  }

  // For other certificate types, use the validation result
  return valid ? 0 : GIT_ECERTIFICATE;
}

// Credential callback for libgit2
static int credential_cb(git_credential **out, const char *url,
                         const char *username_from_url,
                         unsigned int allowed_types, void *payload) {
  CredentialData *cred_data = static_cast<CredentialData *>(payload);

  qDebug() << "Credential callback invoked for URL:" << url;
  qDebug() << "Username from URL:"
           << (username_from_url ? username_from_url : "none");
  qDebug() << "Allowed credential types:" << allowed_types;

  // Try SSH agent first if SSH credentials are allowed
  if (allowed_types & GIT_CREDENTIAL_SSH_KEY) {
    // Always try SSH agent first if it's an option, as it may have multiple
    // keys
    if (cred_data->ssh_agent_tried < 3) { // Allow up to 3 attempts
      cred_data->ssh_agent_tried++;
      qDebug() << "Trying SSH agent authentication (attempt"
               << cred_data->ssh_agent_tried << ")...";
      int error = git_credential_ssh_key_from_agent(
          out, username_from_url ? username_from_url : "git");
      if (error == 0) {
        qDebug() << "SSH agent authentication successful";
        return 0;
      }
      qDebug() << "SSH agent authentication failed, error:" << error;
    }
  }

  // Try default SSH keys if available and not tried yet
  if ((allowed_types & GIT_CREDENTIAL_SSH_KEY) &&
      cred_data->ssh_key_tried == 0) {
    cred_data->ssh_key_tried++;
    qDebug() << "Trying default SSH keys...";
    QString homeDir = QDir::homePath();

    // List of common SSH key pairs to try
    QStringList keyPairs = {
        "id_ed25519", // Modern Ed25519 keys (most common now)
        "id_rsa",     // Traditional RSA keys
        "id_ecdsa",   // ECDSA keys
        "id_dsa"      // Legacy DSA keys
    };

    for (const QString &keyName : keyPairs) {
      QString privateKey = homeDir + "/.ssh/" + keyName;
      QString publicKey = privateKey + ".pub";

      if (QFile::exists(privateKey)) {
        qDebug() << "Trying SSH key:" << keyName;

        // Try without passphrase first (most keys in SSH agents don't have
        // passphrases)
        int error = git_credential_ssh_key_new(
            out, username_from_url ? username_from_url : "git",
            publicKey.toUtf8().constData(), privateKey.toUtf8().constData(),
            "" // Try without passphrase first
        );

        if (error == 0) {
          qDebug() << "SSH key authentication successful with" << keyName;
          return 0;
        }

        qDebug() << "SSH key" << keyName << "failed (error:" << error << ")";

        // If we have a stored passphrase, try with it
        if (!cred_data->passphrase.isEmpty()) {
          qDebug() << "Retrying" << keyName << "with passphrase...";
          error = git_credential_ssh_key_new(
              out, username_from_url ? username_from_url : "git",
              publicKey.toUtf8().constData(), privateKey.toUtf8().constData(),
              cred_data->passphrase.toUtf8().constData());

          if (error == 0) {
            qDebug() << "SSH key authentication successful with" << keyName
                     << "and passphrase";
            return 0;
          }
          qDebug() << "SSH key" << keyName
                   << "failed with passphrase (error:" << error << ")";
        }
      }
    }

    qDebug() << "No working SSH keys found in ~/.ssh/";
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

  qDebug() << "No suitable credential type available or all attempts exhausted";
  qDebug() << "SSH agent tried:" << cred_data->ssh_agent_tried << "times";
  qDebug() << "SSH key tried:" << cred_data->ssh_key_tried << "times";

  // If we tried SSH keys without passphrase and it failed, suggest checking
  // passphrase
  if (cred_data->ssh_key_tried > 0 && cred_data->passphrase.isEmpty()) {
    qDebug() << "Note: If your SSH key is passphrase-protected, authentication "
                "will fail.";
    qDebug()
        << "Consider using ssh-agent or removing the passphrase from your key.";
  }

  // Return an authentication error instead of GIT_PASSTHROUGH
  return GIT_EAUTH;
}

// Progress callback for clone operations
static int transfer_progress_cb(const git_indexer_progress *stats,
                                void *payload) {
  CredentialData *cred_data = static_cast<CredentialData *>(payload);
  GitManager *manager = cred_data ? cred_data->manager : nullptr;

  if (manager) {
    static int last_progress = -1; // Track last reported progress percentage

    if (stats->total_objects > 0) {
      int progress = (stats->received_objects * 100) / stats->total_objects;

      // Only update UI if progress changed by at least 1%
      if (progress != last_progress) {
        QString status = QString("Cloning: %1% (%2/%3 objects)")
                             .arg(progress)
                             .arg(stats->received_objects)
                             .arg(stats->total_objects);
        manager->setStatus(status);
        manager->setProgress(progress);

        // Process events to allow UI updates
        QCoreApplication::processEvents();

        last_progress = progress;
      }
    } else if (stats->received_objects > 0) {
      // During early clone phase when total is not yet known
      // Only update every 100 objects to avoid excessive updates
      static unsigned int last_received = 0;
      if (stats->received_objects - last_received >= 100 ||
          last_received == 0) {
        QString status = QString("Cloning: Receiving objects... (%1 received)")
                             .arg(stats->received_objects);
        manager->setStatus(status);
        manager->setProgress(1); // Show at least 1% to indicate progress

        // Process events to allow UI updates
        QCoreApplication::processEvents();

        last_received = stats->received_objects;
      }
    }
  }
  return 0;
}

GitManager::GitManager(QObject *parent)
    : QObject(parent), m_repositoryUrl(""), m_isBusy(false), m_commitsBehind(0),
      m_fetchIntervalMinutes(60), m_progress(0),
      m_isUrlFromGlobalSettings(false), m_fetchTimer(nullptr) {
  git_libgit2_init();

  loadSettings();

  // Set up the local path based on repository URL
  m_localPath = getRepositoryLocalPath();

  // Initialize fetch timer
  m_fetchTimer = new QTimer(this);
  connect(m_fetchTimer, &QTimer::timeout, this,
          &GitManager::onFetchTimerTimeout);
  startFetchTimer();

  setStatus("Ready");

  // Perform initial check for updates on startup
  // This will clone the repository if it doesn't exist yet
  QTimer::singleShot(1000, this, [this]() {
    qDebug() << "Performing initial check on startup...";
    checkForUpdates();
  });
}

GitManager::~GitManager() {
  stopFetchTimer();
  saveSettings();
  git_libgit2_shutdown();
}

void GitManager::setRepositoryUrl(const QString &url) {
  if (m_repositoryUrl != url) {
    m_repositoryUrl = url;
    m_localPath = getRepositoryLocalPath();
    setCommitsBehind(0);
    saveSettings();
    emit repositoryUrlChanged();
    emit localPathChanged();
  }
}

void GitManager::setLocalPath(const QString &path) {
  if (m_localPath != path) {
    m_localPath = path;
    setCommitsBehind(0);
    saveSettings();
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

void GitManager::setCommitsBehind(int count) {
  if (m_commitsBehind != count) {
    m_commitsBehind = count;
    qDebug() << "Commits behind:" << count;
    emit commitsBehindChanged();
  }
}

void GitManager::setCommitsBehindList(const QVariantList &list) {
  if (m_commitsBehindList != list) {
    m_commitsBehindList = list;
    qDebug() << "Commits behind list updated with" << list.size() << "commits";
    emit commitsBehindListChanged();
  }
}

void GitManager::setFetchIntervalMinutes(int minutes) {
  if (m_fetchIntervalMinutes != minutes && minutes > 0) {
    m_fetchIntervalMinutes = minutes;
    saveSettings();
    emit fetchIntervalMinutesChanged();

    // Restart timer with new interval
    if (m_fetchTimer && m_fetchTimer->isActive()) {
      startFetchTimer();
    }
  }
}

void GitManager::setProgress(int progress) {
  if (m_progress != progress) {
    m_progress = progress;
    qDebug() << "Progress changed to:" << progress;
    emit progressChanged();
  }
}

void GitManager::startFetchTimer() {
  if (m_fetchTimer) {
    m_fetchTimer->stop();
    // Convert minutes to milliseconds
    m_fetchTimer->start(m_fetchIntervalMinutes * 60 * 1000);
    qDebug() << "Fetch timer started with interval:" << m_fetchIntervalMinutes
             << "minutes";
  }
}

void GitManager::stopFetchTimer() {
  if (m_fetchTimer) {
    m_fetchTimer->stop();
  }
}

void GitManager::onFetchTimerTimeout() {
  qDebug() << "Fetch timer triggered, checking for updates...";
  checkForUpdates();
}

void GitManager::checkForUpdates() {
  // Don't check if already busy
  if (m_isBusy) {
    qDebug() << "Already busy, skipping update check";
    return;
  }

  // Check if repository URL is set
  if (m_repositoryUrl.isEmpty()) {
    setStatus("No repository URL set");
    return;
  }

  // Check if repository exists, if not clone it first
  QDir gitDir(m_localPath + "/.git");
  if (!gitDir.exists()) {
    qDebug() << "Repository not cloned yet, cloning now...";
    setIsBusy(true);
    setStatus("Cloning repository...");
    setProgress(1); // Set initial progress to make progress bar visible

    bool success = cloneRepository();
    if (success) {
      setStatus("Repository cloned successfully");
      // After cloning, check commits behind (should be 0)
      int behind = calculateCommitsBehind();
      setCommitsBehind(behind);
    } else {
      setStatus("Failed to clone repository");
    }

    setIsBusy(false);
    return;
  }

  setIsBusy(true);

  bool success = fetchRepository();
  if (success) {
    int behind = calculateCommitsBehind();
    setCommitsBehind(behind);

    // Get detailed commit information
    QVariantList commitDetails = getCommitsBehindDetails();
    setCommitsBehindList(commitDetails);

    if (behind > 0) {
      setStatus(
          QString("Repository is %1 commit(s) behind origin").arg(behind));
    } else {
      setStatus("Repository is up to date");
    }
  }

  setIsBusy(false);
}

void GitManager::loadSettings() {
  QSettings settings("pbek", "nixbit");

  QString savedUrl = settings.value("Repository/Url", QString()).toString();
  if (!savedUrl.isEmpty()) {
    m_repositoryUrl = savedUrl;
  }

  // Load saved local path if available
  QString savedPath =
      settings.value("Repository/LocalPath", QString()).toString();
  if (!savedPath.isEmpty()) {
    m_localPath = savedPath;
  }

  m_fetchIntervalMinutes =
      settings.value("Repository/FetchIntervalMinutes", 60).toInt();
  if (m_fetchIntervalMinutes <= 0) {
    m_fetchIntervalMinutes = 60; // Default to 60 minutes (1 hour)
  }

  // Check for system-wide override from /etc/nixbit.conf
  QSettings systemSettings("/etc/nixbit.conf", QSettings::IniFormat);
  QString systemRepo = systemSettings.value("Repository/Url").toString();
  qDebug() << __func__ << " - 'systemRepo': " << systemRepo;
  if (!systemRepo.isEmpty()) {
    m_repositoryUrl = systemRepo;
    m_isUrlFromGlobalSettings = true;
  }
}

void GitManager::saveSettings() {
  QSettings settings("pbek", "nixbit");
  settings.setValue("Repository/Url", m_repositoryUrl);
  settings.setValue("Repository/LocalPath", m_localPath);
  settings.setValue("Repository/FetchIntervalMinutes", m_fetchIntervalMinutes);
  settings.sync();
}

QString GitManager::getRepositoryLocalPath() const {
  // Use Qt's data location
  const QString dataPath =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  return dataPath + "/repos/nixcfg";
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

  // After successful clone/pull, check how many commits behind we are
  if (success) {
    int behind = calculateCommitsBehind();
    setCommitsBehind(behind);

    // Get detailed commit information
    QVariantList commitDetails = getCommitsBehindDetails();
    setCommitsBehindList(commitDetails);
  }

  setIsBusy(false);
  emit operationCompleted(success, message);
}

void GitManager::pullRepository() {
  if (m_isBusy) {
    qDebug() << "Already busy with an operation";
    return;
  }

  // Check if repository URL is set
  if (m_repositoryUrl.isEmpty()) {
    setStatus("No repository URL set");
    emit operationCompleted(false, "No repository URL set");
    return;
  }

  // Check if repository exists, if not clone it first
  QDir gitDir(m_localPath + "/.git");
  if (!gitDir.exists()) {
    qDebug() << "Repository not cloned yet, cloning before pull...";
    setIsBusy(true);
    setStatus("Cloning repository...");
    setProgress(1); // Set initial progress to make progress bar visible

    bool success = cloneRepository();
    if (success) {
      setStatus("Repository cloned successfully");
      // After cloning, we're up to date
      setCommitsBehind(0);
      setCommitsBehindList(QVariantList());
      emit pullCompletedForUpdate();
    } else {
      setStatus("Failed to clone repository");
      emit operationCompleted(false, "Failed to clone repository");
    }

    setIsBusy(false);
    return;
  }

  setIsBusy(true);
  setStatus("Pulling repository...");

  bool success = pullRepository_internal();
  QString message =
      success ? "Repository updated successfully" : "Failed to pull repository";

  // After successful pull, we should be up to date
  if (success) {
    setCommitsBehind(0);
    setCommitsBehindList(QVariantList());
    emit pullCompletedForUpdate();
  }

  setIsBusy(false);
  emit operationCompleted(success, message);
}

bool GitManager::cloneRepository() {
  // Reset progress for cloning operation
  setProgress(0);

  // Create parent directory if it doesn't exist
  QDir dir;
  QString parentPath = QFileInfo(m_localPath).absolutePath();
  if (!dir.mkpath(parentPath)) {
    setStatus("Failed to create directory");
    return false;
  }

  // Disable global/system git config to avoid insteadOf URL rewrites
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM, "");
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, "");
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, "");

  git_repository *repo = nullptr;
  git_clone_options clone_opts = GIT_CLONE_OPTIONS_INIT;

  // Create credential data structure to track attempts
  CredentialData cred_data;
  cred_data.manager = this;

  // Set up credential callback with tracking
  clone_opts.fetch_opts.callbacks.credentials = credential_cb;
  clone_opts.fetch_opts.callbacks.certificate_check = certificate_check_cb;
  clone_opts.fetch_opts.callbacks.transfer_progress = transfer_progress_cb;
  clone_opts.fetch_opts.callbacks.payload = &cred_data;

  // Set up callbacks for progress
  clone_opts.checkout_opts.checkout_strategy = GIT_CHECKOUT_SAFE;
  clone_opts.checkout_opts.progress_payload = this;

  int error = git_clone(&repo, m_repositoryUrl.toUtf8().constData(),
                        m_localPath.toUtf8().constData(), &clone_opts);

  // Securely clear any stored passphrase
  if (!cred_data.passphrase.isEmpty()) {
    cred_data.passphrase.fill('\0');
    cred_data.passphrase.clear();
  }

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

  // Create credential data structure to track attempts
  CredentialData cred_data;
  cred_data.manager = this;

  // Fetch from remote with credential callback
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
  fetch_opts.callbacks.credentials = credential_cb;
  fetch_opts.callbacks.certificate_check = certificate_check_cb;
  fetch_opts.callbacks.payload = &cred_data;

  error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);

  // Securely clear any stored passphrase
  if (!cred_data.passphrase.isEmpty()) {
    cred_data.passphrase.fill('\0');
    cred_data.passphrase.clear();
  }

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

int GitManager::calculateCommitsBehind() {
  git_repository *repo = nullptr;
  git_oid local_oid, remote_oid;
  git_reference *head_ref = nullptr;
  int error;

  // Open the repository
  error = git_repository_open(&repo, m_localPath.toUtf8().constData());
  if (error != 0) {
    qDebug() << "Failed to open repository for commit count";
    return -1;
  }

  // Get HEAD reference and resolve it to a commit
  error = git_repository_head(&head_ref, repo);
  if (error != 0) {
    qDebug() << "Failed to get HEAD reference";
    git_repository_free(repo);
    return -1;
  }

  // Get the OID that HEAD points to
  const git_oid *head_target = git_reference_target(head_ref);
  if (!head_target) {
    qDebug() << "Failed to get HEAD target OID";
    git_reference_free(head_ref);
    git_repository_free(repo);
    return -1;
  }
  git_oid_cpy(&local_oid, head_target);
  git_reference_free(head_ref);

  // Try to find the remote tracking branch
  const char *remote_branch_names[] = {"refs/remotes/origin/main",
                                       "refs/remotes/origin/master",
                                       "refs/remotes/origin/HEAD"};

  bool found_remote = false;
  for (const char *branch_name : remote_branch_names) {
    error = git_reference_name_to_id(&remote_oid, repo, branch_name);
    if (error == 0) {
      qDebug() << "Found remote branch:" << branch_name;
      found_remote = true;
      break;
    }
  }

  if (!found_remote) {
    qDebug() << "Failed to find remote tracking branch";
    git_repository_free(repo);
    return -1;
  }

  // If commits are the same, we're up to date
  if (git_oid_equal(&local_oid, &remote_oid)) {
    qDebug() << "Local and remote are at the same commit";
    git_repository_free(repo);
    return 0;
  }

  // Use git_graph_ahead_behind to count commits
  size_t ahead, behind;
  error =
      git_graph_ahead_behind(&ahead, &behind, repo, &local_oid, &remote_oid);
  if (error != 0) {
    qDebug() << "Failed to calculate ahead/behind, error:" << error;
    const git_error *e = git_error_last();
    if (e) {
      qDebug() << "Error message:" << e->message;
    }
    git_repository_free(repo);
    return -1;
  }

  int commits_behind = static_cast<int>(behind);
  qDebug() << "Repository is" << ahead << "commit(s) ahead and" << behind
           << "commit(s) behind";

  git_repository_free(repo);
  return commits_behind;
}

bool GitManager::fetchRepository() {
  // Disable global/system git config to avoid insteadOf URL rewrites
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_SYSTEM, "");
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_GLOBAL, "");
  git_libgit2_opts(GIT_OPT_SET_SEARCH_PATH, GIT_CONFIG_LEVEL_XDG, "");

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

  // Create credential data structure to track attempts
  CredentialData cred_data;
  cred_data.manager = this;

  // Fetch from remote with credential callback
  git_fetch_options fetch_opts = GIT_FETCH_OPTIONS_INIT;
  fetch_opts.callbacks.credentials = credential_cb;
  fetch_opts.callbacks.certificate_check = certificate_check_cb;
  fetch_opts.callbacks.payload = &cred_data;

  error = git_remote_fetch(remote, nullptr, &fetch_opts, nullptr);

  // Securely clear any stored passphrase
  if (!cred_data.passphrase.isEmpty()) {
    cred_data.passphrase.fill('\0');
    cred_data.passphrase.clear();
  }

  if (error != 0) {
    const git_error *e = git_error_last();
    QString errorMsg =
        QString("Fetch failed: %1").arg(e ? e->message : "Unknown error");
    setStatus(errorMsg);
    git_remote_free(remote);
    git_repository_free(repo);
    return false;
  }

  // Cleanup
  git_remote_free(remote);
  git_repository_free(repo);

  return true;
}

// Helper function to convert datetime to relative time string
static QString getRelativeTime(const QDateTime &dateTime) {
  qint64 seconds = dateTime.secsTo(QDateTime::currentDateTime());

  if (seconds < 60) {
    return QString::number(seconds) + " seconds ago";
  } else if (seconds < 3600) {
    int minutes = seconds / 60;
    return QString::number(minutes) +
           (minutes == 1 ? " minute ago" : " minutes ago");
  } else if (seconds < 86400) {
    int hours = seconds / 3600;
    return QString::number(hours) + (hours == 1 ? " hour ago" : " hours ago");
  } else if (seconds < 604800) {
    int days = seconds / 86400;
    return QString::number(days) + (days == 1 ? " day ago" : " days ago");
  } else if (seconds < 2592000) {
    int weeks = seconds / 604800;
    return QString::number(weeks) + (weeks == 1 ? " week ago" : " weeks ago");
  } else if (seconds < 31536000) {
    int months = seconds / 2592000;
    return QString::number(months) +
           (months == 1 ? " month ago" : " months ago");
  } else {
    int years = seconds / 31536000;
    return QString::number(years) + (years == 1 ? " year ago" : " years ago");
  }
}

QVariantList GitManager::getCommitsBehindDetails() {
  QVariantList commitList;
  git_repository *repo = nullptr;
  git_oid local_oid, remote_oid;
  git_reference *head_ref = nullptr;
  int error;

  // Open the repository
  error = git_repository_open(&repo, m_localPath.toUtf8().constData());
  if (error != 0) {
    qDebug() << "Failed to open repository for commit details";
    return commitList;
  }

  // Get HEAD reference and resolve it to a commit
  error = git_repository_head(&head_ref, repo);
  if (error != 0) {
    qDebug() << "Failed to get HEAD reference";
    git_repository_free(repo);
    return commitList;
  }

  // Get the OID that HEAD points to
  const git_oid *head_target = git_reference_target(head_ref);
  if (!head_target) {
    qDebug() << "Failed to get HEAD target OID";
    git_reference_free(head_ref);
    git_repository_free(repo);
    return commitList;
  }
  git_oid_cpy(&local_oid, head_target);
  git_reference_free(head_ref);

  // Try to find the remote tracking branch
  const char *remote_branch_names[] = {"refs/remotes/origin/main",
                                       "refs/remotes/origin/master",
                                       "refs/remotes/origin/HEAD"};

  bool found_remote = false;
  for (const char *branch_name : remote_branch_names) {
    error = git_reference_name_to_id(&remote_oid, repo, branch_name);
    if (error == 0) {
      qDebug() << "Found remote branch:" << branch_name;
      found_remote = true;
      break;
    }
  }

  if (!found_remote) {
    qDebug() << "Failed to find remote tracking branch";
    git_repository_free(repo);
    return commitList;
  }

  // If commits are the same, we're up to date
  if (git_oid_equal(&local_oid, &remote_oid)) {
    qDebug() << "Local and remote are at the same commit";
    git_repository_free(repo);
    return commitList;
  }

  // Create a revwalk to iterate through commits
  git_revwalk *walker = nullptr;
  error = git_revwalk_new(&walker, repo);
  if (error != 0) {
    qDebug() << "Failed to create revwalk";
    git_repository_free(repo);
    return commitList;
  }

  // Push the remote OID (where we want to walk to)
  git_revwalk_push(walker, &remote_oid);

  // Hide commits reachable from local HEAD
  git_revwalk_hide(walker, &local_oid);

  // Sort commits by time
  git_revwalk_sorting(walker, GIT_SORT_TIME);

  // Walk through commits
  git_oid oid;
  while (git_revwalk_next(&oid, walker) == 0) {
    git_commit *commit = nullptr;
    error = git_commit_lookup(&commit, repo, &oid);
    if (error != 0) {
      continue;
    }

    // Get commit information
    const char *message = git_commit_message(commit);
    const git_signature *author = git_commit_author(commit);
    git_time_t time = author->when.time;

    // Convert git_time_t to QDateTime
    QDateTime dateTime = QDateTime::fromSecsSinceEpoch(time);

    // Get short SHA
    char short_sha[8];
    git_oid_tostr(short_sha, 8, &oid);

    // Get first line of commit message
    QString fullMessage = QString::fromUtf8(message);
    QString shortMessage = fullMessage.split('\n').first();
    if (shortMessage.length() > 80) {
      shortMessage = shortMessage.left(77) + "...";
    }

    // Create a map for this commit
    QVariantMap commitInfo;
    commitInfo["sha"] = QString(short_sha);
    commitInfo["message"] = shortMessage;
    commitInfo["author"] = QString::fromUtf8(author->name);
    commitInfo["date"] = dateTime.toString("yyyy-MM-dd HH:mm:ss");
    commitInfo["dateRelative"] = getRelativeTime(dateTime);

    commitList.append(commitInfo);

    git_commit_free(commit);
  }

  git_revwalk_free(walker);
  git_repository_free(repo);

  qDebug() << "Retrieved" << commitList.size() << "commits behind";
  return commitList;
}
