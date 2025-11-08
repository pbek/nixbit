#ifndef KONSOLPARTWIDGET_H
#define KONSOLPARTWIDGET_H

#include <KParts/Part>
#include <QQuickItem>

class QQuickWindow;

class KonsolePartWidget : public QQuickItem {
  Q_OBJECT
  Q_PROPERTY(QString workingDirectory READ workingDirectory WRITE
                 setWorkingDirectory NOTIFY workingDirectoryChanged)

public:
  KonsolePartWidget(QQuickItem *parent = nullptr);
  ~KonsolePartWidget();

  QString workingDirectory() const;
  void setWorkingDirectory(const QString &dir);

  void sendCommand(const QString &command);

signals:
  void workingDirectoryChanged();

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  void itemChange(ItemChange change, const ItemChangeData &data) override;

private:
  KParts::Part *m_part;
  QWidget *m_widget;
  QQuickWindow *m_windowContainer;
  QString m_workingDirectory;
};

#endif // KONSOLPARTWIDGET_H
