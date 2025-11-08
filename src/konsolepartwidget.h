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

  Q_INVOKABLE void sendCommand(const QString &command);

signals:
  void workingDirectoryChanged();

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;

private:
  void initializeKonsolePart();
  void updateWidgetPosition();

  KParts::Part *m_part;
  QWidget *m_widget;
  QWidget *m_windowContainer;
  QString m_workingDirectory;
};

#endif // KONSOLPARTWIDGET_H
