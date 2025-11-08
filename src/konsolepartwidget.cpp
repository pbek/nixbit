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

  qDebug() << "QML Window geometry:" << qmlWindow->geometry();
  qDebug() << "QML Window position:" << qmlWindow->position();
  qDebug() << "QML Window global position:"
           << qmlWindow->mapToGlobal(QPoint(0, 0));

  // The widget needs to be shown and positioned manually
  // We'll make it a top-level widget but position it to follow the QML item
  m_widget->setWindowFlags(Qt::Widget | Qt::FramelessWindowHint);

  // Calculate global position on screen
  QPointF scenePos = mapToScene(QPointF(0, 0));
  QPoint globalPos = qmlWindow->mapToGlobal(scenePos.toPoint());

  qDebug() << "Item scene position:" << scenePos;
  qDebug() << "Item global position:" << globalPos;

  // Use a minimum size if the QML item hasn't been sized yet
  int w = qMax(static_cast<int>(width()), 400);
  int h = qMax(static_cast<int>(height()), 300);

  m_widget->setGeometry(globalPos.x(), globalPos.y(), w, h);

  // Create a native window and make it transient to keep it with the main
  // window
  m_widget->setAttribute(Qt::WA_NativeWindow);
  m_widget->winId(); // Force creation of window handle

  if (m_widget->windowHandle()) {
    m_widget->windowHandle()->setTransientParent(qmlWindow);
    qDebug() << "Set widget as transient for main window";
  }

  // Show the widget
  m_widget->show();
  m_widget->raise();

  qDebug() << "KonsolePartWidget: Widget shown at global position:" << globalPos
           << "size:" << w << "x" << h
           << "actual widget geometry:" << m_widget->geometry();

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
    if (m_widget) {
      m_widget->setVisible(isVisible());
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
    // Search all children recursively for TerminalInterface
    QList<QObject *> allChildren = m_part->findChildren<QObject *>();
    qDebug() << "Searching through" << allChildren.size()
             << "child objects for TerminalInterface";

    // First, look for SessionController which should have the session
    QObject *sessionController = nullptr;
    for (QObject *child : allChildren) {
      if (QString(child->metaObject()->className())
              .contains("SessionController")) {
        sessionController = child;
        qDebug() << "Found SessionController:" << child;

        // List all properties to see what's available
        qDebug() << "SessionController properties:";
        const QMetaObject *meta = child->metaObject();
        for (int i = 0; i < meta->propertyCount(); ++i) {
          QMetaProperty prop = meta->property(i);
          qDebug() << "  -" << prop.name() << ":"
                   << child->property(prop.name());
        }

        // List all methods that might help
        qDebug() << "SessionController methods containing 'session':";
        for (int i = 0; i < meta->methodCount(); ++i) {
          QMetaMethod method = meta->method(i);
          QString sig = QString::fromLatin1(method.methodSignature());
          if (sig.contains("session", Qt::CaseInsensitive)) {
            qDebug() << "  -" << sig;
          }
        }

        // Try to get session from the controller using different approaches
        QVariant sessionVar = child->property("session");
        qDebug() << "session property valid:" << sessionVar.isValid()
                 << "type:" << sessionVar.typeName();

        if (sessionVar.isValid()) {
          QObject *session = qvariant_cast<QObject *>(sessionVar);
          if (session) {
            qDebug() << "Found session object:"
                     << session->metaObject()->className();
            terminal = qobject_cast<TerminalInterface *>(session);
            if (terminal) {
              qDebug() << "Session implements TerminalInterface!";
              break;
            } else {
              qWarning() << "Session does not implement TerminalInterface";

              // List session methods
              qDebug() << "Session methods:";
              for (int i = 0; i < session->metaObject()->methodCount(); ++i) {
                QMetaMethod method = session->metaObject()->method(i);
                QString sig = QString::fromLatin1(method.methodSignature());
                if (sig.contains("send", Qt::CaseInsensitive) ||
                    sig.contains("input", Qt::CaseInsensitive) ||
                    sig.contains("text", Qt::CaseInsensitive)) {
                  qDebug() << "  -" << sig;
                }
              }

              // Try sendText method directly on session
              if (QMetaObject::invokeMethod(session, "sendText",
                                            Qt::DirectConnection,
                                            Q_ARG(QString, command + "\n"))) {
                qDebug() << "Command sent via session->sendText";
                return;
              }
              // Try sendInput on session
              if (QMetaObject::invokeMethod(session, "sendInput",
                                            Qt::DirectConnection,
                                            Q_ARG(QString, command + "\n"))) {
                qDebug() << "Command sent via session->sendInput";
                return;
              }
            }
          } else {
            qWarning() << "Failed to cast session property to QObject*";
          }
        } else {
          qWarning() << "session property is not valid";
        }

        // Try calling sendInput directly on the SessionController
        if (QMetaObject::invokeMethod(child, "sendInput", Qt::DirectConnection,
                                      Q_ARG(QString, command + "\n"))) {
          qDebug() << "Command sent via SessionController->sendInput";
          return;
        }

        break;
      }

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
    qWarning() << "Could not send command - TerminalInterface not accessible";
  }
}

void KonsolePartWidget::geometryChange(const QRectF &newGeometry,
                                       const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  updateWidgetPosition();
}

void KonsolePartWidget::updateWidgetPosition() {
  if (m_widget && window()) {
    QPointF scenePos = mapToScene(QPointF(0, 0));
    QPoint globalPos = window()->mapToGlobal(scenePos.toPoint());

    int w = qMax(static_cast<int>(width()), 400);
    int h = qMax(static_cast<int>(height()), 300);

    m_widget->setGeometry(globalPos.x(), globalPos.y(), w, h);
  }
}
