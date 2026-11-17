#ifndef PACKAGESEARCH_H
#define PACKAGESEARCH_H

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSet>
#include <QStringList>
#include <QThread>
#include <QVariantList>
#include <QVector>
#include <memory>

// Searches the packages of nixpkgs. All packages are loaded once with
// "nix search", which uses the evaluation cache of nix after the first time,
// then searching is done locally while typing.
class PackageSearch : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool isIndexing READ isIndexing NOTIFY indexChanged)
  Q_PROPERTY(bool indexReady READ indexReady NOTIFY indexChanged)
  // Estimated progress from 0 to 1 while nixpkgs is evaluated the first time
  Q_PROPERTY(
      double indexProgress READ indexProgress NOTIFY indexProgressChanged)
  Q_PROPERTY(QString indexStatus READ indexStatus NOTIFY indexProgressChanged)
  // True if nix evaluates nixpkgs, which only takes long the first time
  Q_PROPERTY(bool evaluating READ evaluating NOTIFY indexProgressChanged)
  Q_PROPERTY(QString indexError READ indexError NOTIFY indexChanged)
  Q_PROPERTY(int indexSize READ indexSize NOTIFY indexChanged)
  // Version of nixpkgs, e.g. "26.05pre-git", empty if unknown
  Q_PROPERTY(QString nixpkgsVersion READ nixpkgsVersion NOTIFY indexChanged)
  Q_PROPERTY(QString query READ query NOTIFY resultsChanged)
  // Best matches, see search() for the keys
  Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
  // Number of all matches, more than results if they were limited
  Q_PROPERTY(int matchCount READ matchCount NOTIFY resultsChanged)

public:
  explicit PackageSearch(QObject *parent = nullptr);
  ~PackageSearch() override;

  bool isIndexing() const { return m_process != nullptr || m_parsing; }
  bool indexReady() const { return m_entries != nullptr; }
  double indexProgress() const { return m_progress; }
  QString indexStatus() const { return m_status; }
  bool evaluating() const { return m_evaluating; }
  QString indexError() const { return m_error; }
  int indexSize() const { return m_entries ? m_entries->size() : 0; }
  QString nixpkgsVersion() const { return m_nixpkgsVersion; }
  QString query() const { return m_query; }
  QVariantList results() const { return m_results; }
  int matchCount() const { return m_matchCount; }

  void setInstalledAttrNames(const QStringList &names);

public slots:
  // Loads the package index if it isn't loaded yet, or again if forced
  void loadIndex(bool force = false);
  void search(const QString &query);
  void cancelIndexing();

signals:
  void indexChanged();
  void indexProgressChanged();
  void resultsChanged();

private:
  struct Entry {
    QString attrName;
    QString pname;
    QString version;
    QString description;
    // Lower case versions for searching
    QString attrNameLower;
    QString lastNameLower;
    QString pnameLower;
    QString descriptionLower;
  };
  using Entries = QVector<Entry>;

  void onIndexOutput();
  void onIndexFinished(int exitCode, QProcess::ExitStatus status);
  void setProgress(double progress, const QString &status);
  void updateResults();
  static std::shared_ptr<Entries> parseEntries(const QByteArray &json);

  QPointer<QProcess> m_process;
  QPointer<QThread> m_parseThread;
  QByteArray m_output;
  QStringList m_errorLines;
  QString m_partialLine;
  bool m_parsing = false;
  bool m_cancelled = false;
  std::shared_ptr<const Entries> m_entries;
  double m_progress = 0;
  QString m_status;
  bool m_evaluating = false;
  QElapsedTimer m_indexTimer;
  QString m_error;
  QString m_nixpkgsVersion;
  QString m_query;
  QVariantList m_results;
  int m_matchCount = 0;
  QSet<QString> m_installed;
};

#endif // PACKAGESEARCH_H
