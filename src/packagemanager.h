#ifndef PACKAGEMANAGER_H
#define PACKAGEMANAGER_H

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QStringList>
#include <QTimer>
#include <QVariantList>
#include <functional>

#include "utils/nix.h"

// Packages of the user profile that were installed with
// "nix profile add <flake>#<package>", e.g. "nix profile add nixpkgs#hello"
class PackageManager : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool nixAvailable READ nixAvailable CONSTANT)
  Q_PROPERTY(QString profilePath READ profilePath CONSTANT)
  // Packages installed from a flake, see packageToVariant() for the keys
  Q_PROPERTY(QVariantList packages READ packages NOTIFY packagesChanged)
  Q_PROPERTY(int packageCount READ packageCount NOTIFY packagesChanged)
  // Names of profile entries not installed from a flake, e.g. by Home Manager
  Q_PROPERTY(QStringList otherEntries READ otherEntries NOTIFY packagesChanged)
  // Attribute names like "hello" of the packages installed from nixpkgs
  Q_PROPERTY(QStringList installedAttrNames READ installedAttrNames NOTIFY
                 packagesChanged)
  // Size of all packages and their dependencies, -1 if unknown
  Q_PROPERTY(qint64 totalSize READ totalSize NOTIFY packagesChanged)
  Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
  // True once the profile was listed successfully
  Q_PROPERTY(bool loaded READ loaded NOTIFY isLoadingChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)

  Q_PROPERTY(
      bool isCheckingUpdates READ isCheckingUpdates NOTIFY updateCheckChanged)
  Q_PROPERTY(QString updateCheckStatus READ updateCheckStatus NOTIFY
                 updateCheckChanged)
  Q_PROPERTY(
      QString updateCheckError READ updateCheckError NOTIFY updateCheckChanged)
  // Milliseconds since the epoch, 0 if updates weren't checked yet
  Q_PROPERTY(
      double lastUpdateCheck READ lastUpdateCheck NOTIFY updateCheckChanged)
  Q_PROPERTY(int updateCount READ updateCount NOTIFY packagesChanged)

  // Install, update or remove operation
  Q_PROPERTY(bool isBusy READ isBusy NOTIFY operationChanged)
  // "install", "upgrade" or "remove"
  Q_PROPERTY(QString operation READ operation NOTIFY operationChanged)
  // Profile entry names or, for installs, attribute names the operation is
  // for, empty when all packages are updated
  Q_PROPERTY(QStringList operationTargets READ operationTargets NOTIFY
                 operationChanged)
  Q_PROPERTY(QString operationStatus READ operationStatus NOTIFY
                 operationStatusChanged)
  Q_PROPERTY(
      QString operationLog READ operationLog NOTIFY operationStatusChanged)
  // Short description of the running operation, e.g. "Installing hello"
  Q_PROPERTY(QString activityText READ activityText NOTIFY operationChanged)

public:
  explicit PackageManager(QObject *parent = nullptr);
  ~PackageManager() override;

  bool nixAvailable() const;
  QString profilePath() const;
  QVariantList packages() const;
  int packageCount() const { return m_packages.size(); }
  QStringList otherEntries() const { return m_otherEntries; }
  QStringList installedAttrNames() const;
  qint64 totalSize() const { return m_totalSize; }
  bool isLoading() const { return m_isLoading; }
  bool loaded() const { return m_loaded; }
  QString error() const { return m_error; }
  bool isCheckingUpdates() const { return m_isCheckingUpdates; }
  QString updateCheckStatus() const { return m_updateCheckStatus; }
  QString updateCheckError() const { return m_updateCheckError; }
  double lastUpdateCheck() const;
  int updateCount() const;
  bool isBusy() const { return m_operationProcess != nullptr; }
  QString operation() const { return m_operation; }
  QStringList operationTargets() const { return m_operationTargets; }
  QString operationStatus() const { return m_operationStatus; }
  QString operationLog() const { return m_operationLog.join('\n'); }
  QString activityText() const;
  // Packages installed by the system or user configuration, Home Manager or
  // other profile entries, see installationsChanged()
  Utils::Nix::Installations installations() const { return m_installations; }

  // First of the names that the icon theme has an icon for, e.g. the icon of
  // an application named after its program, or an empty string
  Q_INVOKABLE QString themeIconName(const QStringList &candidates) const;

public slots:
  void refresh();
  void checkForUpdates();
  // Checks for updates if the last check is older than the given age
  void checkForUpdatesIfStale(int maxAgeMinutes);

  // Installs a package of nixpkgs, e.g. "hello" or "python3Packages.rich".
  // A priority above the default 5 makes the package lose file conflicts.
  void install(const QString &attrName, bool allowUnfree = false,
               int priority = -1);
  void upgrade(const QStringList &names, bool allowUnfree = false);
  void upgradeAll(bool allowUnfree = false);
  void remove(const QStringList &names);
  void cancelOperation();
  // Repeats the last failed operation with the fix of its hint, see
  // operationFinished()
  void retry(const QString &hint);

signals:
  void packagesChanged();
  void installationsChanged();
  void isLoadingChanged();
  void errorChanged();
  void updateCheckChanged();
  void operationChanged();
  void operationStatusChanged();
  // The hint tells how a failed operation can be retried with retry():
  // "unfree" if a package has an unfree license, "priority" if an installed
  // package has a file of the same name, and empty otherwise
  void operationFinished(bool success, const QString &operation,
                         const QStringList &targets, const QString &message,
                         const QString &hint);

private:
  // Metadata evaluated from the flake a package was installed from
  struct Metadata {
    // Evaluating the metadata was attempted, it may have failed
    bool attempted = false;
    bool valid = false;
    bool missing = false;
    QString pname;
    QString version;
    QString description;
    QString longDescription;
    QString homepage;
    QString mainProgram;
    QStringList licenses;
    bool unfree = false;
    bool insecure = false;
    bool broken = false;
    QStringList outPaths;
  };

  enum class UpdateState { Unknown, UpToDate, Available };

  struct Package {
    QString name;
    QString attrPath;
    QString originalUrl;
    QString url;
    // Explicitly installed outputs, empty for the default outputs
    QStringList outputs;
    QStringList storePaths;
    bool active = true;
    int priority = 5;
    Metadata metadata;
    qint64 closureSize = -1;
    QDateTime installedAt;
    // The package is in the oldest generation still available, so it may have
    // been installed earlier
    bool installedBeforeHistory = false;
    QDateTime updatedAt;
    UpdateState updateState = UpdateState::Unknown;
    QString newVersion;
    QString latestUrl;
  };

  // The latest version of a flake and the packages evaluated from it
  struct LatestSource {
    QString url;
    QHash<QString, Metadata> packages;
  };

  struct EvalRequest {
    QString key;
    QStringList attrPath;
    QStringList outputs;
  };

  void setLoading(bool loading);
  void setError(const QString &error);
  void parseProfile(const QByteArray &json);
  void readHistory();
  void loadSizes(int serial);
  void loadMetadata(int serial);
  void loadInstallations();
  void finishLoadStep(int serial);
  void applyUpdates();
  void finishUpdateCheck();
  void setUpdateCheckStatus(const QString &status);

  // Evaluates the metadata of packages of the flake with the locked URL
  void evaluate(const QString &url, const QList<EvalRequest> &requests,
                const std::function<void(bool, QHash<QString, Metadata>,
                                         const QString &)> &callback);
  static Metadata parseMetadata(const QJsonObject &object);
  static QString metadataKey(const QString &url, const QString &attrPath,
                             const QStringList &outputs);
  static QString requestKey(const QString &attrPath,
                            const QStringList &outputs);
  static QString attrNameOf(const QString &attrPath);
  static QString sourceLabel(const QString &originalUrl);
  static QString revisionOf(const QString &url);
  static QString revisionUrl(const QString &url);

  QVariantMap packageToVariant(const Package &package) const;
  bool anyUnfree(const QStringList &names) const;

  struct OperationRequest {
    QString operation;
    QStringList targets;
    bool allowUnfree = false;
    int priority = -1;
  };

  void startOperation(const OperationRequest &request);
  static QString conflictMessage(const QString &log, const QString &target);
  void onOperationOutput();
  void onOperationFinished(int exitCode, QProcess::ExitStatus status);
  static QString friendlyStatus(const QString &line);

  QList<Package> m_packages;
  QStringList m_otherEntries;
  // Store paths of the other entries, keyed by their name
  QHash<QString, QStringList> m_otherEntryPaths;
  Utils::Nix::Installations m_installations;
  qint64 m_totalSize = -1;
  bool m_isLoading = false;
  bool m_loaded = false;
  bool m_reloadPending = false;
  int m_loadSerial = 0;
  int m_pendingLoadSteps = 0;
  QString m_error;

  QHash<QString, Metadata> m_metadataCache;
  // Keyed by the original flake URL, e.g. "flake:nixpkgs"
  QHash<QString, LatestSource> m_latest;
  bool m_isCheckingUpdates = false;
  int m_pendingUpdateSteps = 0;
  QString m_updateCheckStatus;
  QString m_updateCheckError;
  QStringList m_updateCheckErrors;
  QDateTime m_lastUpdateCheck;
  QTimer m_updateCheckTimer;

  QPointer<QProcess> m_operationProcess;
  OperationRequest m_request;
  QString m_operation;
  QStringList m_operationTargets;
  QString m_operationStatus;
  QStringList m_operationLog;
  QString m_operationPartialLine;
  bool m_operationCancelled = false;
};

#endif // PACKAGEMANAGER_H
