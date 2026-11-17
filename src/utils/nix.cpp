#include "nix.h"
#include <QPointer>
#include <QRegularExpression>
#include <QStandardPaths>

namespace Utils {
namespace Nix {

QString Result::errorMessage() const {
  if (!started) {
    return QStringLiteral("Nix could not be started.");
  }

  const QStringList lines =
      stripAnsi(standardError).split('\n', Qt::SkipEmptyParts);
  // nix prints the error after warnings and progress messages
  for (qsizetype i = lines.size() - 1; i >= 0; --i) {
    const QString line = lines.at(i).trimmed();
    if (line.startsWith(QStringLiteral("error:"))) {
      // Multi-line errors continue on the following, indented lines
      QStringList message{line.mid(6).trimmed()};
      for (qsizetype j = i + 1; j < lines.size() && message.size() < 4; ++j) {
        const QString detail = lines.at(j).trimmed();
        if (detail.isEmpty() || detail.startsWith(QStringLiteral("…")) ||
            detail.startsWith('|') ||
            detail.startsWith(QStringLiteral("at "))) {
          continue;
        }
        message << detail;
      }
      return message.join(' ');
    }
  }
  return lines.isEmpty()
             ? QStringLiteral("Nix failed with exit code %1.").arg(exitCode)
             : lines.last().trimmed();
}

QString executable() {
  static const QString path = QStandardPaths::findExecutable("nix");
  return path;
}

QStringList baseArguments() {
  return {QStringLiteral("--extra-experimental-features"),
          QStringLiteral("nix-command flakes")};
}

QProcess *run(QObject *context, const QStringList &arguments,
              const std::function<void(const Result &)> &callback,
              const QProcessEnvironment &environment) {
  auto *process = new QProcess(context);
  process->setProcessEnvironment(environment);
  QPointer<QObject> guard(context);

  QObject::connect(
      process, &QProcess::finished, context,
      [process, callback, guard](int exitCode, QProcess::ExitStatus status) {
        Result result;
        result.started = true;
        result.exitCode = status == QProcess::CrashExit ? -1 : exitCode;
        result.standardOutput = process->readAllStandardOutput();
        result.standardError =
            QString::fromUtf8(process->readAllStandardError());
        process->deleteLater();
        if (guard) {
          callback(result);
        }
      });
  QObject::connect(process, &QProcess::errorOccurred, context,
                   [process, callback, guard](QProcess::ProcessError error) {
                     if (error != QProcess::FailedToStart) {
                       return;
                     }
                     Result result;
                     result.standardError = process->errorString();
                     process->deleteLater();
                     if (guard) {
                       callback(result);
                     }
                   });

  const QString nix = executable();
  process->start(nix.isEmpty() ? QStringLiteral("nix") : nix,
                 baseArguments() + arguments);
  return process;
}

QString stripAnsi(QString text) {
  static const QRegularExpression escapes(
      QStringLiteral("\\x1B\\[[0-?]*[ -/]*[@-~]|\\x1B\\][^\\x07]*\\x07"));
  return text.remove(escapes);
}

QString stringLiteral(const QString &value) {
  QString escaped = value;
  escaped.replace('\\', QStringLiteral("\\\\"));
  escaped.replace('"', QStringLiteral("\\\""));
  escaped.replace('$', QStringLiteral("\\$"));
  escaped.replace('\n', QStringLiteral("\\n"));
  escaped.replace('\r', QStringLiteral("\\r"));
  escaped.replace('\t', QStringLiteral("\\t"));
  return '"' + escaped + '"';
}

QStringList splitAttrPath(const QString &attrPath) {
  QStringList parts;
  QString current;
  bool quoted = false;
  for (qsizetype i = 0; i < attrPath.size(); ++i) {
    const QChar c = attrPath.at(i);
    if (c == '"') {
      quoted = !quoted;
    } else if (c == '\\' && quoted && i + 1 < attrPath.size()) {
      current += attrPath.at(++i);
    } else if (c == '.' && !quoted) {
      parts << current;
      current.clear();
    } else {
      current += c;
    }
  }
  parts << current;
  return parts;
}

StoreName parseStoreName(const QString &storePath) {
  // /nix/store/<32 character hash>-<name>
  const QString baseName = storePath.section('/', -1);
  const QString name = baseName.mid(baseName.indexOf('-') + 1);

  // Like builtins.parseDrvName, the version starts at the first dash
  // followed by a digit
  static const QRegularExpression versionStart(QStringLiteral("-(?=\\d)"));
  const QRegularExpressionMatch match = versionStart.match(name);
  if (!match.hasMatch()) {
    return {name, QString()};
  }
  return {name.left(match.capturedStart()), name.mid(match.capturedEnd())};
}

QString versionFromStorePath(const QString &storePath) {
  QString version = parseStoreName(storePath).version;
  if (version.isEmpty()) {
    return version;
  }

  // Drop the output name of e.g. jq-1.8.2-bin
  static const QRegularExpression outputSuffix(QStringLiteral(
      "-(bin|out|man|dev|doc|devdoc|info|lib|libexec|share|data|debug|"
      "terminfo|static)$"));
  version.remove(outputSuffix);
  return version;
}

} // namespace Nix
} // namespace Utils
