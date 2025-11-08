#include "konsolepartwidget.h"
#include <KParts/Part>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDebug>
#include <QQuickWindow>
#include <QVBoxLayout>
#include <QWidget>
#include <konsole/private/terminalinterface.h>

KonsolePartWidget::KonsolePartWidget(QQuickItem *parent)
    : QQuickItem(parent), m_part(nullptr), m_widget(nullptr),
      m_windowContainer(nullptr) {
  setFlag(ItemHasContents, true);

  // We need to wait for the window to be available
  connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
    if (window && !m_part) {
      initializeKonsolePart();
    }
  });
}

void KonsolePartWidget::initializeKonsolePart() {
  qDebug() << "KonsolePartWidget: initializeKonsolePart called";

  // Try to load the konsolepart using different possible plugin names
  QStringList pluginNames = {
      QStringLiteral("konsolepart"), QStringLiteral("org.kde.konsole.part"),
      QStringLiteral("kf6/parts/konsolepart"),
      QStringLiteral("kf5/parts/konsolepart"), QStringLiteral("konsole")};

  KPluginMetaData metaData;
  bool found = false;

  for (const QString &pluginName : pluginNames) {
    metaData = KPluginMetaData(pluginName);
    if (metaData.isValid()) {
      qDebug() << "Found KonsolePart plugin with name:" << pluginName;
      found = true;
      break;
    } else {
      qDebug() << "Plugin not found with name:" << pluginName;
    }
  }

  if (!found) {
    qWarning()
        << "Failed to find KonsolePart plugin metadata with any known name";
    qWarning() << "Trying to load from library directly...";

    // Try to load the library directly
    auto result = KPluginFactory::loadFactory(
        KPluginMetaData(QStringLiteral("konsolepart")));
    if (!result) {
      qWarning() << "Failed to load factory:" << result.errorString;
      return;
    }

    m_part = result.plugin->create<KParts::Part>(this);
    if (!m_part) {
      qWarning() << "Failed to create KonsolePart from factory";
      return;
    }
  } else {
    auto result =
        KPluginFactory::instantiatePlugin<KParts::Part>(metaData, this);
    if (!result) {
      qWarning() << "Failed to load KonsolePart:" << result.errorString;
      return;
    }
    m_part = result.plugin;
  }

  if (!m_part) {
    qWarning() << "Failed to load KonsolePart";
    return;
  }

  qDebug() << "KonsolePartWidget: KonsolePart loaded successfully";

  m_widget = m_part->widget();
  if (!m_widget) {
    qWarning() << "KonsolePartWidget: Failed to get widget from part";
    return;
  }

  // Initialize the part - this is important for konsole to be ready
  // Try to access the TerminalInterface to verify it's available
  TerminalInterface *terminalTest = qobject_cast<TerminalInterface *>(m_part);
  if (terminalTest) {
    qDebug() << "TerminalInterface is accessible from part";
  } else {
    qWarning() << "TerminalInterface is NOT accessible from part - this may "
                  "cause issues";
    qWarning() << "Available interfaces:";
    for (const QMetaObject *mo = m_part->metaObject(); mo;
         mo = mo->superClass()) {
      qWarning() << "  -" << mo->className();
    }
  }

  if (!window()) {
    qWarning() << "KonsolePartWidget: No window available";
    return;
  }

  qDebug() << "KonsolePartWidget: Setting up widget embedding";

  // Get the QQuickWindow
  QQuickWindow *qmlWindow = window();

  // Create a native widget that will contain the konsole widget
  // and be positioned as an overlay on the QML window
  m_windowContainer = new QWidget();

  // Set it to be a frameless window that overlays the main window
  m_windowContainer->setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);
  m_windowContainer->setAttribute(Qt::WA_TranslucentBackground, false);

  // Create layout and add the konsole widget
  QVBoxLayout *layout = new QVBoxLayout(m_windowContainer);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(0);
  layout->addWidget(m_widget);

  // Calculate global position on screen
  QPointF scenePos = mapToScene(QPointF(0, 0));
  QPoint globalPos = qmlWindow->mapToGlobal(scenePos.toPoint());

  // Position and size the widget
  // Use a minimum size if the QML item hasn't been sized yet
  int w = qMax(static_cast<int>(width()), 400);
  int h = qMax(static_cast<int>(height()), 300);

  m_windowContainer->setGeometry(globalPos.x(), globalPos.y(), w, h);

  // Create a native window for the container
  m_windowContainer->setAttribute(Qt::WA_NativeWindow);
  m_windowContainer->winId(); // Force creation of window handle

  // Make the widget window transient for the QML window so they stay together
  if (m_windowContainer->windowHandle()) {
    m_windowContainer->windowHandle()->setTransientParent(qmlWindow);
  }

  // Show the widget
  m_windowContainer->show();
  m_windowContainer->raise();

  qDebug() << "KonsolePartWidget: Initial widget size:" << w << "x" << h
           << "at global position:" << globalPos;

  // Connect to geometry changes
  connect(this, &QQuickItem::xChanged, this,
          &KonsolePartWidget::updateWidgetPosition);
  connect(this, &QQuickItem::yChanged, this,
          &KonsolePartWidget::updateWidgetPosition);
  connect(this, &QQuickItem::widthChanged, this,
          &KonsolePartWidget::updateWidgetPosition);
  connect(this, &QQuickItem::heightChanged, this,
          &KonsolePartWidget::updateWidgetPosition);

  // Connect to window position changes to keep widget in sync
  connect(qmlWindow, &QQuickWindow::xChanged, this,
          &KonsolePartWidget::updateWidgetPosition);
  connect(qmlWindow, &QQuickWindow::yChanged, this,
          &KonsolePartWidget::updateWidgetPosition);

  // Connect to item visibility changes
  connect(this, &QQuickItem::visibleChanged, this, [this]() {
    if (m_windowContainer) {
      m_windowContainer->setVisible(isVisible());
    }
  });

  qDebug() << "KonsolePartWidget: Terminal widget positioned at scene"
           << scenePos << "size:" << width() << "x" << height();
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
  qDebug() << "KonsolePartWidget::sendCommand called with:" << command;

  if (!m_part) {
    qWarning() << "Cannot send command: m_part is null";
    return;
  }

  // Search for TerminalInterface in the part and all its descendants
  TerminalInterface *terminal = nullptr;

  // Try the part itself first
  terminal = qobject_cast<TerminalInterface *>(m_part);

  if (!terminal) {
    // Search all children recursively
    QList<QObject *> allChildren = m_part->findChildren<QObject *>();
    qDebug() << "Searching through" << allChildren.size()
             << "child objects for TerminalInterface";

    for (QObject *child : allChildren) {
      terminal = qobject_cast<TerminalInterface *>(child);
      if (terminal) {
        qDebug() << "Found TerminalInterface in child:"
                 << child->metaObject()->className();
        break;
      }
    }
  }

  if (terminal) {
    qDebug() << "Sending command via TerminalInterface:" << command;
    terminal->sendInput(command + "\n");
    qDebug() << "Command sent successfully";
  } else {
    qWarning() << "TerminalInterface not found anywhere in part hierarchy";

    // Try calling methods that might exist on Konsole::Part
    QStringList methodsToTry = {"sendText", "sendInput", "writeToTerminal",
                                "sendData"};
    bool sent = false;

    for (const QString &methodName : methodsToTry) {
      if (QMetaObject::invokeMethod(m_part, methodName.toUtf8().constData(),
                                    Qt::DirectConnection,
                                    Q_ARG(QString, command + "\n"))) {
        qDebug() << "Command sent via" << methodName;
        sent = true;
        break;
      }
    }

    if (!sent) {
      qWarning() << "Could not find any way to send command to terminal";
      qWarning() << "Available methods on" << m_part->metaObject()->className()
                 << ":";
      for (int i = 0; i < m_part->metaObject()->methodCount(); ++i) {
        QMetaMethod method = m_part->metaObject()->method(i);
        if (method.access() == QMetaMethod::Public &&
            method.methodType() == QMetaMethod::Method) {
          qWarning() << "  -" << method.methodSignature();
        }
      }
    }
  }
}

void KonsolePartWidget::geometryChange(const QRectF &newGeometry,
                                       const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  updateWidgetPosition();
}

void KonsolePartWidget::updateWidgetPosition() {
  if (m_windowContainer && window()) {
    QPointF scenePos = mapToScene(QPointF(0, 0));
    QPoint globalPos = window()->mapToGlobal(scenePos.toPoint());

    int w = qMax(static_cast<int>(width()), 400);
    int h = qMax(static_cast<int>(height()), 300);

    m_windowContainer->setGeometry(globalPos.x(), globalPos.y(), w, h);
  }
}
