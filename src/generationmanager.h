#ifndef GENERATIONMANAGER_H
#define GENERATIONMANAGER_H

#include <QAbstractListModel>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>

struct Generation {
  int number;
  bool isCurrent;
  QString dateTime;
  QString nixosVersion;
  QString kernelVersion;
  QString configurationRevision;
};

class GenerationManager : public QAbstractListModel {
  Q_OBJECT
  Q_PROPERTY(bool isLoading READ isLoading NOTIFY isLoadingChanged)
  Q_PROPERTY(QString error READ error NOTIFY errorChanged)
  Q_PROPERTY(int currentGenerationNumber READ currentGenerationNumber NOTIFY
                 currentGenerationChanged)
  Q_PROPERTY(QString currentGenerationDate READ currentGenerationDate NOTIFY
                 currentGenerationChanged)

public:
  enum GenerationRoles {
    NumberRole = Qt::UserRole + 1,
    IsCurrentRole,
    DateTimeRole,
    NixosVersionRole,
    KernelVersionRole,
    ConfigurationRevisionRole
  };

  explicit GenerationManager(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  bool isLoading() const { return m_isLoading; }
  QString error() const { return m_error; }
  int currentGenerationNumber() const { return m_currentGenerationNumber; }
  QString currentGenerationDate() const { return m_currentGenerationDate; }

public slots:
  void loadGenerations();
  void setActive(bool active);

signals:
  void isLoadingChanged();
  void errorChanged();
  void generationsLoaded();
  void currentGenerationChanged();

private:
  void parseGenerations(const QString &output);
  QString formatDateTime(const QString &timestamp);

  QList<Generation> m_generations;
  bool m_isLoading;
  QString m_error;
  QTimer *m_refreshTimer;
  int m_currentGenerationNumber;
  QString m_currentGenerationDate;
};

#endif // GENERATIONMANAGER_H
