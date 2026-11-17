#pragma once

#include <QHash>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <functional>

namespace Utils {
namespace Nix {

struct Result {
  int exitCode = -1;
  bool started = false;
  QByteArray standardOutput;
  QString standardError;

  bool ok() const { return started && exitCode == 0; }
  // Last error line of nix, without the "error:" prefix
  QString errorMessage() const;
};

// Path of the nix binary, empty if nix isn't installed
QString executable();

// Arguments enabling the experimental features "nix profile" and flakes need,
// so they also work where they aren't enabled in nix.conf
QStringList baseArguments();

// Runs nix asynchronously and calls the callback with the result. The process
// is owned by the context and dropped without a callback if it is destroyed.
QProcess *run(QObject *context, const QStringList &arguments,
              const std::function<void(const Result &)> &callback,
              const QProcessEnvironment &environment =
                  QProcessEnvironment::systemEnvironment());

// Removes terminal escape sequences, e.g. colors, from nix output
QString stripAnsi(QString text);

// Quotes a string as a Nix string literal
QString stringLiteral(const QString &value);

// Splits an attribute path like legacyPackages.x86_64-linux."a.b" into its
// attribute names
QStringList splitAttrPath(const QString &attrPath);

// Version part of a store path name like /nix/store/<hash>-jq-1.8.2-bin
QString versionFromStorePath(const QString &storePath);

// Name and version of a store path like /nix/store/<hash>-jq-1.8.2-bin, split
// like builtins.parseDrvName, so the version may end with an output name
struct StoreName {
  QString name;
  QString version;
};
StoreName parseStoreName(const QString &storePath);

// A package installed other than with "nix profile add", e.g. by the system
// configuration
struct Installation {
  // Short label, e.g. "system config"
  QString source;
  // Description, e.g. "your system configuration"
  QString description;
  QString version;

  bool operator==(const Installation &other) const {
    return source == other.source && description == other.description &&
           version == other.version;
  }
};
// Installations keyed by the lower case package name
using Installations = QHash<QString, QList<Installation>>;

} // namespace Nix
} // namespace Utils
