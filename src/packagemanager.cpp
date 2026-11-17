#include "packagemanager.h"
#include "utils/nix.h"
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSet>
#include <algorithm>
#include <sys/stat.h>

namespace {

// Maximum number of output lines kept for an operation
constexpr int maxOperationLogLines = 3000;
// Interval of the automatic update check
constexpr int updateCheckIntervalHours = 6;

// Evaluates the metadata of packages of a flake. Packages of nixpkgs are
// evaluated with unfree, broken and insecure packages allowed, otherwise their
// store paths can't be compared to find updates.
const char *metadataExpression = R"nix(
let
  request = builtins.fromJSON @REQUEST@;
  flake = builtins.getFlake request.url;
  try = value: fallback:
    let result = builtins.tryEval value;
    in if result.success then result.value else fallback;
  str = value: if builtins.isString value then value else null;
  toList = value: if builtins.isList value then value else [ value ];
  isNixpkgs = flake ? legacyPackages
    && builtins.pathExists (flake.outPath + "/pkgs/top-level/all-packages.nix");
  nixpkgsFor = builtins.mapAttrs (system: _: import flake.outPath {
    inherit system;
    config = {
      allowUnfree = true;
      allowBroken = true;
      allowUnsupportedSystem = true;
      allowInsecurePredicate = _: true;
    };
  }) (if isNixpkgs then flake.legacyPackages else { });
  lookup = path:
    let
      fromNixpkgs = isNixpkgs && builtins.length path > 2
        && builtins.head path == "legacyPackages";
      root = if fromNixpkgs then nixpkgsFor.${builtins.elemAt path 1} else flake;
      names = if fromNixpkgs
        then builtins.genList (i: builtins.elemAt path (i + 2)) (builtins.length path - 2)
        else path;
    in builtins.foldl' (set: name:
      if set != null && builtins.isAttrs set && set ? ${name} then set.${name} else null
    ) root names;
  licenseName = license:
    if builtins.isAttrs license
    then license.spdxId or license.shortName or license.fullName or null
    else str license;
  info = package:
    let
      pkg = try (lookup package.attrPath) null;
      meta = try (pkg.meta or { }) { };
      parsedName = try (builtins.parseDrvName pkg.name) { name = null; version = null; };
      licenses = try (toList (meta.license or [ ])) [ ];
      outputs = if package.outputs != [ ] then package.outputs
        else try (meta.outputsToInstall or [ (pkg.outputName or "out") ]) [ "out" ];
    in
    if pkg == null || !(builtins.isAttrs pkg) then { missing = true; } else {
      missing = false;
      pname = try (str (pkg.pname or parsedName.name)) null;
      version = try (str (pkg.version or parsedName.version)) null;
      description = try (str (meta.description or null)) null;
      longDescription = try (str (meta.longDescription or null)) null;
      homepage = try (str (builtins.head (toList (meta.homepage or null)))) null;
      mainProgram = try (str (meta.mainProgram or null)) null;
      licenses = try (builtins.filter (name: name != null) (map licenseName licenses)) [ ];
      unfree = try (builtins.any (license: builtins.isAttrs license && !(license.free or true)) licenses) false;
      insecure = try ((meta.knownVulnerabilities or [ ]) != [ ]) false;
      broken = try (meta.broken or false) false;
      outPaths = try (map (output: pkg.${output}.outPath) outputs) null;
    };
in
builtins.mapAttrs (key: package: info package) request.packages
)nix";

QString profileLink() {
  const QFileInfo homeProfile(QDir::homePath() + "/.nix-profile");
  if (homeProfile.isSymLink()) {
    return homeProfile.symLinkTarget();
  }
  const QString stateHome = qEnvironmentVariable(
      "XDG_STATE_HOME", QDir::homePath() + "/.local/state");
  return stateHome + "/nix/profiles/profile";
}

QDateTime linkTime(const QString &path) {
  struct stat st;
  if (lstat(QFile::encodeName(path).constData(), &st) != 0) {
    return QDateTime();
  }
  return QDateTime::fromSecsSinceEpoch(st.st_mtime);
}

QStringList toStringList(const QJsonValue &value) {
  QStringList list;
  for (const QJsonValue &item : value.toArray()) {
    list << item.toString();
  }
  return list;
}

QString storeName(const QString &path) { return path.section('/', -1); }

// Name of a store path without the hash, e.g. "hello-2.12.3"
QString storeDisplayName(const QString &path) {
  const QString name = storeName(path);
  const qsizetype dash = name.indexOf('-');
  return dash >= 0 ? name.mid(dash + 1) : name;
}

double toMilliseconds(const QDateTime &dateTime) {
  return dateTime.isValid() ? double(dateTime.toMSecsSinceEpoch()) : 0.0;
}

} // namespace

PackageManager::PackageManager(QObject *parent) : QObject(parent) {
  m_updateCheckTimer.setInterval(updateCheckIntervalHours * 3600 * 1000);
  connect(&m_updateCheckTimer, &QTimer::timeout, this,
          &PackageManager::checkForUpdates);
}

PackageManager::~PackageManager() {
  if (m_operationProcess) {
    m_operationProcess->disconnect(this);
    m_operationProcess->terminate();
    m_operationProcess->waitForFinished(3000);
  }
}

bool PackageManager::nixAvailable() const {
  return !Utils::Nix::executable().isEmpty();
}

QString PackageManager::profilePath() const {
  const QFileInfo homeProfile(QDir::homePath() + "/.nix-profile");
  return homeProfile.isSymLink() ? QStringLiteral("~/.nix-profile")
                                 : profileLink();
}

QVariantList PackageManager::packages() const {
  QVariantList list;
  for (const Package &package : m_packages) {
    list << packageToVariant(package);
  }
  return list;
}

QStringList PackageManager::installedAttrNames() const {
  QStringList names;
  for (const Package &package : m_packages) {
    if (sourceLabel(package.originalUrl).startsWith("nixpkgs") ||
        package.originalUrl.contains("NixOS/nixpkgs", Qt::CaseInsensitive)) {
      names << attrNameOf(package.attrPath);
    }
  }
  return names;
}

double PackageManager::lastUpdateCheck() const {
  return toMilliseconds(m_lastUpdateCheck);
}

int PackageManager::updateCount() const {
  return std::count_if(m_packages.cbegin(), m_packages.cend(),
                       [](const Package &package) {
                         return package.updateState == UpdateState::Available;
                       });
}

QString PackageManager::activityText() const {
  if (!isBusy()) {
    return QString();
  }
  const QString target = m_operationTargets.size() == 1
                             ? m_operationTargets.first()
                             : QStringLiteral("packages");
  if (m_operation == "install") {
    return QStringLiteral("Installing %1").arg(target);
  }
  if (m_operation == "remove") {
    return QStringLiteral("Removing %1").arg(target);
  }
  return QStringLiteral("Updating %1").arg(target);
}

QString PackageManager::themeIconName(const QStringList &candidates) const {
  for (const QString &candidate : candidates) {
    if (!candidate.isEmpty() && QIcon::hasThemeIcon(candidate)) {
      return candidate;
    }
  }
  return QString();
}

void PackageManager::setLoading(bool loading) {
  if (m_isLoading != loading) {
    m_isLoading = loading;
    emit isLoadingChanged();
  }
  if (!loading && m_reloadPending) {
    m_reloadPending = false;
    refresh();
  }
}

void PackageManager::setError(const QString &error) {
  if (m_error != error) {
    m_error = error;
    emit errorChanged();
  }
}

void PackageManager::refresh() {
  if (!nixAvailable()) {
    setError("Nix wasn't found. Packages can only be managed if the nix "
             "command is in your PATH.");
    return;
  }
  if (m_isLoading) {
    m_reloadPending = true;
    return;
  }

  const int serial = ++m_loadSerial;
  setLoading(true);

  Utils::Nix::run(
      this, {"profile", "list", "--json"},
      [this, serial](const Utils::Nix::Result &result) {
        if (serial != m_loadSerial) {
          return;
        }
        if (!result.ok()) {
          QString message = result.errorMessage();
          if (message.contains("nix-env") ||
              message.contains("incompatible", Qt::CaseInsensitive)) {
            message = "Your profile was created with nix-env, so it can't be "
                      "managed with \"nix profile\". " +
                      message;
          }
          setError(message);
          setLoading(false);
          return;
        }

        setError(QString());
        const bool firstLoad = !m_loaded;
        parseProfile(result.standardOutput);
        loadInstallations();
        readHistory();
        applyUpdates();
        m_loaded = true;
        emit packagesChanged();
        emit isLoadingChanged();

        // Check for updates in the background after the first load
        if (firstLoad && !m_packages.isEmpty()) {
          QTimer::singleShot(60 * 1000, this,
                             [this]() { checkForUpdatesIfStale(60); });
          m_updateCheckTimer.start();
        }

        m_pendingLoadSteps = 2;
        loadSizes(serial);
        loadMetadata(serial);
      });
}

void PackageManager::finishLoadStep(int serial) {
  if (serial != m_loadSerial) {
    return;
  }
  if (--m_pendingLoadSteps <= 0) {
    setLoading(false);
  }
}

void PackageManager::parseProfile(const QByteArray &json) {
  const QJsonObject root = QJsonDocument::fromJson(json).object();

  // Keep the sizes of unchanged packages until they are loaded again
  QHash<QString, Package> previous;
  for (const Package &package : std::as_const(m_packages)) {
    previous.insert(package.name, package);
  }

  QList<Package> packages;
  QStringList otherEntries;
  QHash<QString, QStringList> otherEntryPaths;

  const auto addElement = [&](const QString &name, const QJsonObject &element) {
    Package package;
    package.name = name;
    package.attrPath = element.value("attrPath").toString();
    package.originalUrl = element.value("originalUrl").toString();
    package.url = element.value("url").toString();
    package.outputs = toStringList(element.value("outputs"));
    package.storePaths = toStringList(element.value("storePaths"));
    package.active = element.value("active").toBool(true);
    package.priority = element.value("priority").toInt(5);

    if (package.attrPath.isEmpty() || package.originalUrl.isEmpty()) {
      otherEntries << name;
      otherEntryPaths.insert(name, package.storePaths);
      return;
    }

    const auto old = previous.constFind(name);
    if (old != previous.cend() && old->storePaths == package.storePaths) {
      package.closureSize = old->closureSize;
    }
    package.metadata = m_metadataCache.value(
        metadataKey(package.url, package.attrPath, package.outputs));
    packages << package;
  };

  const QJsonValue elements = root.value("elements");
  if (elements.isObject()) {
    const QJsonObject object = elements.toObject();
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
      addElement(it.key(), it.value().toObject());
    }
  } else {
    // Manifest version 2 has no names, nix uses the last attribute name
    for (const QJsonValue &value : elements.toArray()) {
      const QJsonObject element = value.toObject();
      QString name =
          Utils::Nix::splitAttrPath(element.value("attrPath").toString())
              .constLast();
      if (name.isEmpty()) {
        const QStringList storePaths =
            toStringList(element.value("storePaths"));
        name = storePaths.isEmpty() ? QStringLiteral("unknown")
                                    : storeDisplayName(storePaths.first());
      }
      addElement(name, element);
    }
  }

  std::sort(packages.begin(), packages.end(),
            [](const Package &a, const Package &b) {
              return QString::compare(a.name, b.name, Qt::CaseInsensitive) < 0;
            });
  otherEntries.sort(Qt::CaseInsensitive);

  m_packages = packages;
  m_otherEntries = otherEntries;
  m_otherEntryPaths = otherEntryPaths;
}

void PackageManager::loadInstallations() {
  // Environments whose packages are the store paths they reference, e.g. the
  // system-path of environment.systemPackages
  struct Environment {
    QString source;
    QString description;
  };
  QHash<QString, Environment> environments;
  const auto addEnvironment = [&](const QString &path, const QString &source,
                                  const QString &description) {
    const QString storePath = QFileInfo(path).canonicalFilePath();
    if (storePath.startsWith("/nix/store/") &&
        !environments.contains(storePath)) {
      environments.insert(storePath, {source, description});
    }
  };

  addEnvironment("/run/current-system/sw", "system config",
                 "your system configuration");
  const QString user = qEnvironmentVariable("USER");
  if (!user.isEmpty()) {
    addEnvironment("/etc/profiles/per-user/" + user, "user config",
                   "your NixOS user configuration");
  }
  for (auto it = m_otherEntryPaths.constBegin();
       it != m_otherEntryPaths.constEnd(); ++it) {
    const bool homeManager = it.key().contains("home-manager");
    for (const QString &path : it.value()) {
      addEnvironment(
          path, homeManager ? QStringLiteral("Home Manager") : it.key(),
          homeManager ? QStringLiteral("Home Manager")
                      : QStringLiteral("the profile entry %1").arg(it.key()));
    }
  }

  if (environments.isEmpty()) {
    if (!m_installations.isEmpty()) {
      m_installations.clear();
      emit installationsChanged();
    }
    return;
  }

  Utils::Nix::run(
      this, QStringList{"path-info", "--json"} + environments.keys(),
      [this, environments](const Utils::Nix::Result &result) {
        if (!result.ok()) {
          qWarning() << "Failed to read installed environments:"
                     << result.errorMessage();
          return;
        }

        // References of every environment, keyed by its store path name
        QHash<QString, QJsonArray> references;
        const QJsonDocument document =
            QJsonDocument::fromJson(result.standardOutput);
        if (document.isObject()) {
          const QJsonObject object = document.object();
          for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            references.insert(
                storeName(it.key()),
                it.value().toObject().value("references").toArray());
          }
        } else {
          for (const QJsonValue &value : document.array()) {
            const QJsonObject info = value.toObject();
            references.insert(storeName(info.value("path").toString()),
                              info.value("references").toArray());
          }
        }

        Utils::Nix::Installations installations;
        for (auto it = environments.constBegin(); it != environments.constEnd();
             ++it) {
          QSet<QString> seen;
          for (const QJsonValue &reference :
               references.value(storeName(it.key()))) {
            const QString path = reference.toString();
            const QString key = Utils::Nix::parseStoreName(path).name.toLower();
            const QString version = Utils::Nix::versionFromStorePath(path);
            // Outputs of the same package, e.g. jq-1.8.2-bin and jq-1.8.2-man
            if (version.isEmpty() || seen.contains(key + '-' + version)) {
              continue;
            }
            seen.insert(key + '-' + version);
            installations[key].append(
                {it.value().source, it.value().description, version});
          }
        }

        m_installations = installations;
        emit installationsChanged();
      });
}

void PackageManager::readHistory() {
  // Every change of the profile creates a generation, e.g. profile-12-link,
  // whose manifest lists the packages of that generation
  const QFileInfo link(profileLink());
  const QDir directory(link.absolutePath());
  const QRegularExpression generationRe(
      "^" + QRegularExpression::escape(link.fileName()) + "-(\\d+)-link$");

  struct Generation {
    int number;
    QString path;
  };
  QList<Generation> generations;
  const QStringList entries = directory.entryList(
      QDir::AllEntries | QDir::System | QDir::NoDotAndDotDot);
  for (const QString &entry : entries) {
    const QRegularExpressionMatch match = generationRe.match(entry);
    if (match.hasMatch()) {
      generations.append(
          {match.captured(1).toInt(), directory.filePath(entry)});
    }
  }
  std::sort(generations.begin(), generations.end(),
            [](const Generation &a, const Generation &b) {
              return a.number < b.number;
            });

  QHash<QString, QDateTime> installedAt;
  QHash<QString, QDateTime> updatedAt;
  QSet<QString> inFirstGeneration;
  QHash<QString, QString> previous;

  for (qsizetype i = 0; i < generations.size(); ++i) {
    const Generation &generation = generations.at(i);
    QFile manifest(generation.path + "/manifest.json");
    if (!manifest.open(QIODevice::ReadOnly)) {
      continue;
    }
    const QJsonObject elements = QJsonDocument::fromJson(manifest.readAll())
                                     .object()
                                     .value("elements")
                                     .toObject();
    const QDateTime time = linkTime(generation.path);

    QHash<QString, QString> current;
    for (auto it = elements.constBegin(); it != elements.constEnd(); ++it) {
      QStringList storePaths =
          toStringList(it.value().toObject().value("storePaths"));
      storePaths.sort();
      current.insert(it.key(), storePaths.join('\n'));
    }

    for (auto it = current.constBegin(); it != current.constEnd(); ++it) {
      const auto old = previous.constFind(it.key());
      if (old == previous.cend()) {
        installedAt.insert(it.key(), time);
        updatedAt.remove(it.key());
        if (i == 0 && generation.number > 1) {
          inFirstGeneration.insert(it.key());
        } else {
          inFirstGeneration.remove(it.key());
        }
      } else if (old.value() != it.value()) {
        updatedAt.insert(it.key(), time);
      }
    }
    previous = current;
  }

  for (Package &package : m_packages) {
    package.installedAt = installedAt.value(package.name);
    package.updatedAt = updatedAt.value(package.name);
    package.installedBeforeHistory = inFirstGeneration.contains(package.name);
  }
}

void PackageManager::loadSizes(int serial) {
  QStringList storePaths;
  for (const Package &package : std::as_const(m_packages)) {
    storePaths << package.storePaths;
  }
  if (storePaths.isEmpty()) {
    m_totalSize = 0;
    finishLoadStep(serial);
    return;
  }

  Utils::Nix::run(
      this, QStringList{"path-info", "--json", "--recursive"} + storePaths,
      [this, serial](const Utils::Nix::Result &result) {
        if (serial != m_loadSerial) {
          return;
        }
        if (!result.ok()) {
          qWarning() << "Failed to read package sizes:"
                     << result.errorMessage();
          finishLoadStep(serial);
          return;
        }

        // Size and references of every path, keyed by the store path name,
        // because newer nix versions list references without the store dir
        QHash<QString, qint64> sizes;
        QHash<QString, QStringList> references;
        const auto addInfo = [&](const QString &path, const QJsonObject &info) {
          const QString name = storeName(path);
          sizes.insert(name, info.value("narSize").toInteger());
          QStringList refs;
          for (const QJsonValue &reference :
               info.value("references").toArray()) {
            refs << storeName(reference.toString());
          }
          references.insert(name, refs);
        };

        const QJsonDocument document =
            QJsonDocument::fromJson(result.standardOutput);
        if (document.isObject()) {
          const QJsonObject object = document.object();
          for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (it.value().isObject()) {
              addInfo(it.key(), it.value().toObject());
            }
          }
        } else {
          for (const QJsonValue &value : document.array()) {
            const QJsonObject info = value.toObject();
            addInfo(info.value("path").toString(), info);
          }
        }

        const auto closureOf = [&](const QStringList &paths) {
          QSet<QString> seen;
          QStringList queue;
          for (const QString &path : paths) {
            queue << storeName(path);
          }
          qint64 size = 0;
          while (!queue.isEmpty()) {
            const QString name = queue.takeLast();
            if (seen.contains(name)) {
              continue;
            }
            seen.insert(name);
            size += sizes.value(name);
            queue << references.value(name);
          }
          return size;
        };

        QStringList allPaths;
        for (Package &package : m_packages) {
          package.closureSize = closureOf(package.storePaths);
          allPaths << package.storePaths;
        }
        m_totalSize = closureOf(allPaths);
        emit packagesChanged();
        finishLoadStep(serial);
      });
}

void PackageManager::loadMetadata(int serial) {
  // Packages are evaluated from the locked flake they were installed from
  QHash<QString, QList<EvalRequest>> requestsByUrl;
  for (const Package &package : std::as_const(m_packages)) {
    if (package.url.isEmpty() ||
        m_metadataCache.contains(
            metadataKey(package.url, package.attrPath, package.outputs))) {
      continue;
    }
    QList<EvalRequest> &requests = requestsByUrl[package.url];
    const QString key = requestKey(package.attrPath, package.outputs);
    const bool known =
        std::any_of(requests.cbegin(), requests.cend(),
                    [&key](const EvalRequest &r) { return r.key == key; });
    if (!known) {
      requests.append(
          {key, Utils::Nix::splitAttrPath(package.attrPath), package.outputs});
    }
  }

  if (requestsByUrl.isEmpty()) {
    finishLoadStep(serial);
    return;
  }

  auto pending = std::make_shared<int>(requestsByUrl.size());
  for (auto it = requestsByUrl.constBegin(); it != requestsByUrl.constEnd();
       ++it) {
    const QString url = it.key();
    QSet<QString> keys;
    for (const EvalRequest &request : it.value()) {
      keys.insert(request.key);
    }
    evaluate(url, it.value(),
             [this, serial, url, keys,
              pending](bool ok, const QHash<QString, Metadata> &result,
                       const QString &error) {
               if (!ok) {
                 qWarning() << "Failed to evaluate package metadata:" << error;
               }
               // Failures are cached too, so they aren't evaluated again
               for (const QString &key : keys) {
                 Metadata metadata = result.value(key);
                 metadata.attempted = true;
                 m_metadataCache.insert(url + '#' + key, metadata);
               }
               for (Package &package : m_packages) {
                 const QString key =
                     requestKey(package.attrPath, package.outputs);
                 if (package.url == url && keys.contains(key)) {
                   package.metadata = m_metadataCache.value(url + '#' + key);
                 }
               }
               if (serial == m_loadSerial) {
                 emit packagesChanged();
               }
               if (--*pending == 0) {
                 finishLoadStep(serial);
               }
             });
  }
}

void PackageManager::evaluate(
    const QString &url, const QList<EvalRequest> &requests,
    const std::function<void(bool, QHash<QString, Metadata>, const QString &)>
        &callback) {
  QJsonObject packages;
  for (const EvalRequest &request : requests) {
    packages.insert(
        request.key,
        QJsonObject{{"attrPath", QJsonArray::fromStringList(request.attrPath)},
                    {"outputs", QJsonArray::fromStringList(request.outputs)}});
  }
  const QJsonObject request{{"url", url}, {"packages", packages}};
  const QString json =
      QString::fromUtf8(QJsonDocument(request).toJson(QJsonDocument::Compact));
  QString expression = QString::fromUtf8(metadataExpression);
  expression.replace("@REQUEST@", Utils::Nix::stringLiteral(json));

  // --impure allows evaluating flakes that aren't locked, e.g. local paths
  Utils::Nix::run(
      this, {"eval", "--impure", "--json", "--expr", expression},
      [callback](const Utils::Nix::Result &result) {
        QHash<QString, Metadata> metadata;
        if (!result.ok()) {
          callback(false, metadata, result.errorMessage());
          return;
        }
        const QJsonObject object =
            QJsonDocument::fromJson(result.standardOutput).object();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
          metadata.insert(it.key(), parseMetadata(it.value().toObject()));
        }
        callback(true, metadata, QString());
      });
}

PackageManager::Metadata
PackageManager::parseMetadata(const QJsonObject &object) {
  Metadata metadata;
  metadata.valid = true;
  metadata.missing = object.value("missing").toBool();
  metadata.pname = object.value("pname").toString();
  metadata.version = object.value("version").toString();
  metadata.description = object.value("description").toString().trimmed();
  metadata.longDescription =
      object.value("longDescription").toString().trimmed();
  metadata.homepage = object.value("homepage").toString();
  metadata.mainProgram = object.value("mainProgram").toString();
  metadata.licenses = toStringList(object.value("licenses"));
  metadata.unfree = object.value("unfree").toBool();
  metadata.insecure = object.value("insecure").toBool();
  metadata.broken = object.value("broken").toBool();
  metadata.outPaths = toStringList(object.value("outPaths"));
  metadata.outPaths.sort();
  return metadata;
}

QString PackageManager::requestKey(const QString &attrPath,
                                   const QStringList &outputs) {
  return outputs.isEmpty() ? attrPath : attrPath + '^' + outputs.join(',');
}

QString PackageManager::metadataKey(const QString &url, const QString &attrPath,
                                    const QStringList &outputs) {
  return url + '#' + requestKey(attrPath, outputs);
}

void PackageManager::checkForUpdatesIfStale(int maxAgeMinutes) {
  if (m_lastUpdateCheck.isValid() &&
      m_lastUpdateCheck.secsTo(QDateTime::currentDateTime()) <
          maxAgeMinutes * 60) {
    return;
  }
  checkForUpdates();
}

void PackageManager::setUpdateCheckStatus(const QString &status) {
  m_updateCheckStatus = status;
  emit updateCheckChanged();
}

void PackageManager::checkForUpdates() {
  if (m_isCheckingUpdates || !nixAvailable() || !m_loaded) {
    return;
  }

  QStringList origins;
  for (const Package &package : std::as_const(m_packages)) {
    if (!origins.contains(package.originalUrl)) {
      origins << package.originalUrl;
    }
  }
  if (origins.isEmpty()) {
    m_lastUpdateCheck = QDateTime::currentDateTime();
    m_updateCheckError.clear();
    emit updateCheckChanged();
    return;
  }

  m_isCheckingUpdates = true;
  m_updateCheckErrors.clear();
  m_pendingUpdateSteps = origins.size();
  setUpdateCheckStatus(origins.size() == 1
                           ? QStringLiteral("Fetching the latest %1…")
                                 .arg(sourceLabel(origins.first()))
                           : QStringLiteral("Fetching the latest versions…"));

  const auto finishStep = [this]() {
    if (--m_pendingUpdateSteps == 0) {
      finishUpdateCheck();
    }
  };

  for (const QString &origin : std::as_const(origins)) {
    // Resolve the newest revision of the flake, like "nix profile upgrade"
    Utils::Nix::run(
        this, {"flake", "metadata", "--json", "--refresh", origin},
        [this, origin, finishStep](const Utils::Nix::Result &result) {
          if (!result.ok()) {
            m_updateCheckErrors << QStringLiteral("%1: %2").arg(
                sourceLabel(origin), result.errorMessage());
            finishStep();
            return;
          }
          const QString latestUrl =
              QJsonDocument::fromJson(result.standardOutput)
                  .object()
                  .value("url")
                  .toString();
          if (latestUrl.isEmpty()) {
            m_updateCheckErrors
                << QStringLiteral("%1: The latest revision is unknown.")
                       .arg(sourceLabel(origin));
            finishStep();
            return;
          }

          // Only packages installed from an older revision need evaluating
          QList<EvalRequest> requests;
          for (const Package &package : std::as_const(m_packages)) {
            if (package.originalUrl != origin || package.url == latestUrl) {
              continue;
            }
            const QString key = requestKey(package.attrPath, package.outputs);
            const bool known = std::any_of(
                requests.cbegin(), requests.cend(),
                [&key](const EvalRequest &r) { return r.key == key; });
            if (!known) {
              requests.append({key, Utils::Nix::splitAttrPath(package.attrPath),
                               package.outputs});
            }
          }

          if (requests.isEmpty()) {
            m_latest.insert(origin, LatestSource{latestUrl, {}});
            finishStep();
            return;
          }

          setUpdateCheckStatus(
              requests.size() == 1
                  ? QStringLiteral("Looking for a newer version…")
                  : QStringLiteral("Comparing %1 packages with %2…")
                        .arg(requests.size())
                        .arg(sourceLabel(origin)));
          evaluate(latestUrl, requests,
                   [this, origin, latestUrl,
                    finishStep](bool ok, const QHash<QString, Metadata> &result,
                                const QString &error) {
                     if (ok) {
                       m_latest.insert(origin, LatestSource{latestUrl, result});
                     } else {
                       m_updateCheckErrors << QStringLiteral("%1: %2").arg(
                           sourceLabel(origin), error);
                     }
                     finishStep();
                   });
        });
  }
}

void PackageManager::finishUpdateCheck() {
  m_isCheckingUpdates = false;
  m_lastUpdateCheck = QDateTime::currentDateTime();
  m_updateCheckStatus.clear();
  m_updateCheckError = m_updateCheckErrors.join('\n');
  applyUpdates();
  emit packagesChanged();
  emit updateCheckChanged();
}

void PackageManager::applyUpdates() {
  for (Package &package : m_packages) {
    package.updateState = UpdateState::Unknown;
    package.newVersion.clear();
    package.latestUrl.clear();

    const auto latest = m_latest.constFind(package.originalUrl);
    if (latest == m_latest.cend()) {
      continue;
    }
    package.latestUrl = latest->url;
    if (package.url == latest->url) {
      package.updateState = UpdateState::UpToDate;
      continue;
    }

    const auto metadata = latest->packages.constFind(
        requestKey(package.attrPath, package.outputs));
    if (metadata == latest->packages.cend() || !metadata->valid ||
        metadata->missing || metadata->outPaths.isEmpty()) {
      continue;
    }
    QStringList installed = package.storePaths;
    installed.sort();
    if (installed == metadata->outPaths) {
      package.updateState = UpdateState::UpToDate;
    } else {
      package.updateState = UpdateState::Available;
      package.newVersion = metadata->version;
    }
  }
}

QString PackageManager::attrNameOf(const QString &attrPath) {
  const QStringList parts = Utils::Nix::splitAttrPath(attrPath);
  if (parts.size() > 2 &&
      (parts.first() == "legacyPackages" || parts.first() == "packages")) {
    return parts.mid(2).join('.');
  }
  return attrPath;
}

QString PackageManager::sourceLabel(const QString &originalUrl) {
  QString label = originalUrl;
  if (label.startsWith("flake:")) {
    label = label.mid(6);
  }
  return label;
}

QString PackageManager::revisionOf(const QString &url) {
  static const QRegularExpression revisionRe(
      QStringLiteral("(?:[?&]rev=|^[a-z]+:[^/?]+/[^/?]+/)([0-9a-f]{40})"));
  const QRegularExpressionMatch match = revisionRe.match(url);
  return match.hasMatch() ? match.captured(1) : QString();
}

QString PackageManager::revisionUrl(const QString &url) {
  static const QRegularExpression forgeRe(
      QStringLiteral("^(github|gitlab|sourcehut):([^/?]+)/([^/?]+)/"
                     "([0-9a-f]{40})"));
  const QRegularExpressionMatch match = forgeRe.match(url);
  if (!match.hasMatch()) {
    return QString();
  }
  const QString forge = match.captured(1);
  const QString path = match.captured(2) + '/' + match.captured(3);
  const QString revision = match.captured(4);
  if (forge == "gitlab") {
    return "https://gitlab.com/" + path + "/-/commit/" + revision;
  }
  if (forge == "sourcehut") {
    return "https://git.sr.ht/" + path + "/commit/" + revision;
  }
  return "https://github.com/" + path + "/commit/" + revision;
}

QVariantMap PackageManager::packageToVariant(const Package &package) const {
  const Metadata &metadata = package.metadata;
  const QString attrName = attrNameOf(package.attrPath);
  const QString version =
      !metadata.version.isEmpty()
          ? metadata.version
          : (package.storePaths.isEmpty() ? QString()
                                          : Utils::Nix::versionFromStorePath(
                                                package.storePaths.first()));
  const QString source = sourceLabel(package.originalUrl);
  const QString revision = revisionOf(package.url);
  const QString latestRevision = revisionOf(package.latestUrl);

  QString updateState = QStringLiteral("unknown");
  if (package.updateState == UpdateState::UpToDate) {
    updateState = QStringLiteral("upToDate");
  } else if (package.updateState == UpdateState::Available) {
    updateState = QStringLiteral("available");
  }

  QString installReference = package.originalUrl;
  if (installReference.startsWith("flake:")) {
    installReference = installReference.mid(6);
  }
  installReference += '#' + attrName;
  if (!package.outputs.isEmpty()) {
    installReference += '^' + package.outputs.join(',');
  }

  QVariantMap map;
  map["name"] = package.name;
  map["attrName"] = attrName;
  map["attrPath"] = package.attrPath;
  map["originalUrl"] = package.originalUrl;
  map["url"] = package.url;
  map["source"] = source;
  map["revision"] = revision.left(7);
  map["revisionUrl"] = revisionUrl(package.url);
  map["installCommand"] = "nix profile add " + installReference;
  map["storePaths"] = package.storePaths;
  map["outputs"] = package.outputs;
  map["active"] = package.active;
  map["priority"] = package.priority;
  map["metadataLoaded"] = metadata.attempted || metadata.valid;
  map["missing"] = metadata.missing;
  map["pname"] = metadata.pname;
  map["version"] = version;
  map["description"] = metadata.description;
  map["longDescription"] = metadata.longDescription;
  map["homepage"] = metadata.homepage;
  map["mainProgram"] = metadata.mainProgram;
  map["licenses"] = metadata.licenses;
  map["unfree"] = metadata.unfree;
  map["insecure"] = metadata.insecure;
  map["broken"] = metadata.broken;
  map["closureSize"] = double(package.closureSize);
  map["installedAt"] = toMilliseconds(package.installedAt);
  map["installedBeforeHistory"] = package.installedBeforeHistory;
  map["updatedAt"] = toMilliseconds(package.updatedAt);
  map["updateState"] = updateState;
  map["newVersion"] = package.newVersion;
  map["isRebuild"] = package.updateState == UpdateState::Available &&
                     !package.newVersion.isEmpty() &&
                     package.newVersion == version;
  map["latestRevision"] = latestRevision.left(7);
  map["latestRevisionUrl"] = revisionUrl(package.latestUrl);
  return map;
}

bool PackageManager::anyUnfree(const QStringList &names) const {
  return std::any_of(m_packages.cbegin(), m_packages.cend(),
                     [&names](const Package &package) {
                       return package.metadata.unfree &&
                              (names.isEmpty() || names.contains(package.name));
                     });
}

void PackageManager::install(const QString &attrName, bool allowUnfree,
                             int priority) {
  static const QRegularExpression validAttrName(
      QStringLiteral("^[A-Za-z0-9_][A-Za-z0-9_.+'-]*$"));
  if (!validAttrName.match(attrName).hasMatch()) {
    emit operationFinished(
        false, "install", {attrName},
        QStringLiteral("\"%1\" isn't a valid package name.").arg(attrName),
        QString());
    return;
  }
  startOperation({"install", {attrName}, allowUnfree, priority});
}

void PackageManager::upgrade(const QStringList &names, bool allowUnfree) {
  if (names.isEmpty()) {
    return;
  }
  startOperation({"upgrade", names, allowUnfree || anyUnfree(names)});
}

void PackageManager::upgradeAll(bool allowUnfree) {
  startOperation({"upgrade", {}, allowUnfree || anyUnfree({})});
}

void PackageManager::remove(const QStringList &names) {
  if (names.isEmpty()) {
    return;
  }
  startOperation({"remove", names});
}

void PackageManager::retry(const QString &hint) {
  OperationRequest request = m_request;
  if (request.operation.isEmpty() || isBusy()) {
    return;
  }
  if (hint == "unfree") {
    request.allowUnfree = true;
  } else if (hint == "priority" && request.operation == "install") {
    // Packages of the default priority 5 win the conflict
    request.priority = 6;
  } else {
    return;
  }
  startOperation(request);
}

void PackageManager::cancelOperation() {
  if (m_operationProcess) {
    m_operationCancelled = true;
    // nix cleans up after SIGTERM, e.g. releases the profile lock
    m_operationProcess->terminate();
    QTimer::singleShot(5000, m_operationProcess,
                       [process = m_operationProcess]() {
                         if (process) {
                           process->kill();
                         }
                       });
  }
}

void PackageManager::startOperation(const OperationRequest &request) {
  if (isBusy() || !nixAvailable()) {
    return;
  }

  const QString &operation = request.operation;
  const QStringList &targets = request.targets;
  QStringList arguments{"profile"};
  if (operation == "install") {
    arguments << "add" << "nixpkgs#" + targets.value(0);
    if (request.priority >= 0) {
      arguments << "--priority" << QString::number(request.priority);
    }
  } else if (operation == "remove") {
    arguments << "remove" << targets;
  } else if (targets.isEmpty()) {
    arguments << "upgrade" << "--all";
  } else {
    arguments << "upgrade" << targets;
  }
  if (request.allowUnfree) {
    // nixpkgs only reads the environment variable in impure evaluation
    arguments << "--impure";
  }

  QProcessEnvironment environment = QProcessEnvironment::systemEnvironment();
  if (request.allowUnfree) {
    environment.insert("NIXPKGS_ALLOW_UNFREE", "1");
  }
  QStringList fullArguments = Utils::Nix::baseArguments() + arguments;
  if (operation != "remove") {
    fullArguments << "--print-build-logs";
  }

  auto *process = new QProcess(this);
  process->setProcessEnvironment(environment);
  process->setProcessChannelMode(QProcess::MergedChannels);
  connect(process, &QProcess::readyReadStandardOutput, this,
          &PackageManager::onOperationOutput);
  connect(process, &QProcess::finished, this,
          &PackageManager::onOperationFinished);
  connect(process, &QProcess::errorOccurred, this,
          [this, process](QProcess::ProcessError error) {
            if (error == QProcess::FailedToStart &&
                process == m_operationProcess) {
              m_operationLog << process->errorString();
              onOperationFinished(-1, QProcess::CrashExit);
            }
          });

  m_operationProcess = process;
  m_request = request;
  m_operation = operation;
  m_operationTargets = targets;
  m_operationLog = {(request.allowUnfree
                         ? QStringLiteral("$ NIXPKGS_ALLOW_UNFREE=1 nix ")
                         : QStringLiteral("$ nix ")) +
                    arguments.join(' ')};
  m_operationPartialLine.clear();
  m_operationCancelled = false;
  m_operationStatus = operation == "remove"
                          ? QStringLiteral("Removing…")
                          : QStringLiteral("Evaluating packages…");
  emit operationChanged();
  emit operationStatusChanged();

  process->start(Utils::Nix::executable(), fullArguments);
}

void PackageManager::onOperationOutput() {
  if (!m_operationProcess) {
    return;
  }
  QString text = m_operationPartialLine +
                 QString::fromUtf8(m_operationProcess->readAllStandardOutput());
  text.replace('\r', '\n');
  QStringList lines = text.split('\n');
  m_operationPartialLine = lines.takeLast();

  bool changed = false;
  for (const QString &rawLine : std::as_const(lines)) {
    const QString line = Utils::Nix::stripAnsi(rawLine).trimmed();
    if (line.isEmpty()) {
      continue;
    }
    m_operationLog << line;
    const QString status = friendlyStatus(line);
    if (!status.isEmpty()) {
      m_operationStatus = status;
    }
    changed = true;
  }
  if (m_operationLog.size() > maxOperationLogLines) {
    m_operationLog.erase(m_operationLog.begin(),
                         m_operationLog.begin() +
                             (m_operationLog.size() - maxOperationLogLines));
  }
  if (changed) {
    emit operationStatusChanged();
  }
}

QString PackageManager::friendlyStatus(const QString &line) {
  static const QRegularExpression copyingRe(
      QStringLiteral("^copying path '([^']+)' from"));
  static const QRegularExpression buildingRe(
      QStringLiteral("^building '([^']+)\\.drv'"));
  static const QRegularExpression fetchSummaryRe(
      QStringLiteral("^th(?:is path|ese (\\d+) paths) will be fetched "
                     "\\(([^ ]+ [^ ,]+) download"));
  static const QRegularExpression buildSummaryRe(
      QStringLiteral("^th(?:is derivation|ese (\\d+) derivations) will be "
                     "built"));
  static const QRegularExpression buildLogRe(
      QStringLiteral("^([A-Za-z0-9._+-]+)> "));

  QRegularExpressionMatch match = copyingRe.match(line);
  if (match.hasMatch()) {
    return QStringLiteral("Downloading %1…")
        .arg(storeDisplayName(match.captured(1)));
  }
  match = buildingRe.match(line);
  if (match.hasMatch()) {
    return QStringLiteral("Building %1…")
        .arg(storeDisplayName(match.captured(1)));
  }
  match = fetchSummaryRe.match(line);
  if (match.hasMatch()) {
    const QString count = match.captured(1);
    return count.isEmpty()
               ? QStringLiteral("Downloading %1…").arg(match.captured(2))
               : QStringLiteral("Downloading %1 paths (%2)…")
                     .arg(count, match.captured(2));
  }
  match = buildSummaryRe.match(line);
  if (match.hasMatch()) {
    const QString count = match.captured(1);
    return count.isEmpty() ? QStringLiteral("Building 1 package…")
                           : QStringLiteral("Building %1 packages…").arg(count);
  }
  match = buildLogRe.match(line);
  if (match.hasMatch()) {
    return QStringLiteral("Building %1…").arg(match.captured(1));
  }
  if (line.startsWith("unpacking ") || line.startsWith("fetching ") ||
      line.startsWith("downloading ")) {
    return QStringLiteral("Fetching sources…");
  }
  if (line.startsWith("evaluating")) {
    return QStringLiteral("Evaluating packages…");
  }
  if (line.startsWith("upgrading ")) {
    return QStringLiteral("Upgrading…");
  }
  if (line.startsWith("removing ") || line.startsWith("removed ")) {
    return QStringLiteral("Removing…");
  }
  if (line.startsWith("error:")) {
    return line.mid(6).trimmed();
  }
  // Store path lists, warnings and other details don't make a good status
  return QString();
}

void PackageManager::onOperationFinished(int exitCode,
                                         QProcess::ExitStatus status) {
  QProcess *process = m_operationProcess;
  if (!process) {
    return;
  }
  onOperationOutput();
  if (!m_operationPartialLine.trimmed().isEmpty()) {
    m_operationLog << Utils::Nix::stripAnsi(m_operationPartialLine).trimmed();
    m_operationPartialLine.clear();
  }

  const QString operation = m_operation;
  const QStringList targets = m_operationTargets;
  const bool success =
      !m_operationCancelled && status == QProcess::NormalExit && exitCode == 0;
  const QString log = m_operationLog.join('\n');
  const bool failed = !success && !m_operationCancelled;
  const bool needsUnfree = failed && log.contains("unfree license");
  const QString conflict = failed && !needsUnfree && operation == "install"
                               ? conflictMessage(log, targets.value(0))
                               : QString();
  const QString subject =
      targets.size() == 1 ? targets.first()
                          : QStringLiteral("%1 packages").arg(targets.size());

  QString message;
  if (success) {
    if (operation == "install") {
      message = QStringLiteral("%1 was installed.").arg(subject);
    } else if (operation == "remove") {
      message = QStringLiteral("%1 %2 removed.")
                    .arg(subject, targets.size() == 1 ? "was" : "were");
    } else if (targets.isEmpty()) {
      message = QStringLiteral("All packages were updated.");
    } else {
      message = QStringLiteral("%1 %2 updated.")
                    .arg(subject, targets.size() == 1 ? "was" : "were");
    }
  } else if (m_operationCancelled) {
    message = QStringLiteral("The operation was cancelled.");
  } else if (needsUnfree) {
    message =
        QStringLiteral("%1 has an unfree license, which Nix refuses to "
                       "install unless you allow it.")
            .arg(targets.isEmpty() ? QStringLiteral("A package") : subject);
  } else if (!conflict.isEmpty()) {
    message = conflict;
  } else {
    Utils::Nix::Result result;
    result.started = status == QProcess::NormalExit;
    result.exitCode = exitCode;
    result.standardError = log;
    message = result.errorMessage();
  }

  m_operationProcess = nullptr;
  m_operation.clear();
  m_operationTargets.clear();
  m_operationStatus.clear();
  process->deleteLater();
  emit operationChanged();
  emit operationStatusChanged();
  const QString hint = needsUnfree           ? QStringLiteral("unfree")
                       : !conflict.isEmpty() ? QStringLiteral("priority")
                                             : QString();
  emit operationFinished(success, operation, targets, message, hint);

  refresh();
}

QString PackageManager::conflictMessage(const QString &log,
                                        const QString &target) {
  // nix: An existing package already provides the following file:
  // "/nix/store/<hash>-rar-7.23/bin/unrar" This is the conflicting file from
  // the new package: "/nix/store/<hash>-unrar-7.3.2/bin/unrar"
  static const QRegularExpression conflictRe(
      QStringLiteral("already provides the following file:\\s*\"?(/nix/store/"
                     "[^/\"\\s]+)/([^\"\\s]+)\"?[\\s\\S]*?conflicting file "
                     "from the new package:\\s*\"?(/nix/store/[^/\"\\s]+)/"));
  const QRegularExpressionMatch match = conflictRe.match(log);
  if (!match.hasMatch()) {
    return QString();
  }

  // Which of the packages nix calls "existing" depends on their order in the
  // profile, so the other package is the one not named like the new one
  const QString first = storeDisplayName(match.captured(1));
  const QString second = storeDisplayName(match.captured(3));
  const QString lastName = target.section('.', -1);
  const QString other = first.startsWith(lastName + '-') ? second : first;
  return QStringLiteral("%1 can't be installed next to %2, because both "
                        "contain the file %3.")
      .arg(target, other, match.captured(2));
}
