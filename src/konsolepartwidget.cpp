#include "konsolepartwidget.h"
#include <KParts/PartLoader>
#include <QDebug>
#include <QQuickWindow>
#include <QVBoxLayout>
#include <QWidget>
#include <konsole/private/terminalinterface.h>

KonsolePartWidget::KonsolePartWidget(QQuickItem *parent)
    : QQuickItem(parent), m_part(nullptr), m_widget(nullptr),
      m_windowContainer(nullptr) {
  setFlag(ItemHasContents, true);

  // Load the konsolepart using PartLoader
  m_part = KParts::PartLoader::loadPart(this, "konsolepart", this);
  if (!m_part) {
    qWarning() << "Failed to load KonsolePart";
    return;
  }

  m_widget = m_part->widget();
  if (m_widget) {
    m_widget->setVisible(true);
  }
}

KonsolePartWidget::~KonsolePartWidget() {
  if (m_windowContainer) {
    delete m_windowContainer;
  }
}

QString KonsolePartWidget::workingDirectory() const {
  return m_workingDirectory;
}

void KonsolePartWidget::setWorkingDirectory(const QString &dir) {
  if (m_workingDirectory != dir) {
    m_workingDirectory = dir;
    emit workingDirectoryChanged();
    // TODO: Set working directory in the terminal if possible
  }
}

void KonsolePartWidget::sendCommand(const QString &command) {
  if (m_part) {
    // Try to get the TerminalInterface
    TerminalInterface *terminal = qobject_cast<TerminalInterface *>(m_part);
    if (terminal) {
      terminal->sendInput(command + "\n");
      qDebug() << "Sent command to terminal:" << command;
    } else {
      qWarning() << "TerminalInterface not available";
    }
  }
}

void KonsolePartWidget::geometryChange(const QRectF &newGeometry,
                                       const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  if (m_windowContainer) {
    m_windowContainer->setGeometry(newGeometry.toRect());
  }
}

void KonsolePartWidget::itemChange(ItemChange change,
                                   const ItemChangeData &data) {
  if (change == ItemSceneChange) {
    if (data.window && m_widget) {
      // Create a window container for the QWidget
      m_windowContainer = QWidget::createWindowContainer(m_widget, nullptr);
      m_windowContainer->setParent(data.window);
      m_windowContainer->setGeometry(boundingRect().toRect());
      m_windowContainer->show();
    }
  }
  QQuickItem::itemChange(change, data);
}
