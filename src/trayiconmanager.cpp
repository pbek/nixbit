#include "trayiconmanager.h"
#include <QApplication>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#include <QDebug>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QTransform>

namespace {
const QString notificationsService =
    QStringLiteral("org.freedesktop.Notifications");
const QString notificationsPath =
    QStringLiteral("/org/freedesktop/Notifications");
const QString notificationsInterface =
    QStringLiteral("org.freedesktop.Notifications");
} // namespace

TrayIconManager::TrayIconManager(bool debugMode, QObject *parent)
    : QObject(parent), m_trayIcon(nullptr), m_trayMenu(nullptr),
      m_currentCommitsBehind(-1), m_debugMode(debugMode), m_notificationId(0) {
  createTrayIcon();

  m_activityTimer.setInterval(100);
  connect(&m_activityTimer, &QTimer::timeout, this, [this]() {
    m_activityFrame = (m_activityFrame + 1) % m_activityFrames.size();
    m_trayIcon->setIcon(m_activityFrames.at(m_activityFrame));
  });

  QDBusConnection bus = QDBusConnection::sessionBus();
  bus.connect(notificationsService, notificationsPath, notificationsInterface,
              "ActionInvoked", this,
              SLOT(onNotificationActionInvoked(uint, QString)));
  bus.connect(notificationsService, notificationsPath, notificationsInterface,
              "ActivationToken", this,
              SLOT(onNotificationActivationToken(uint, QString)));
  bus.connect(notificationsService, notificationsPath, notificationsInterface,
              "NotificationClosed", this,
              SLOT(onNotificationClosed(uint, uint)));
}

TrayIconManager::~TrayIconManager() {
  if (m_trayIcon) {
    m_trayIcon->hide();
    delete m_trayIcon;
  }
  if (m_trayMenu) {
    delete m_trayMenu;
  }
}

void TrayIconManager::createTrayIcon() {
  // Create tray menu
  m_trayMenu = new QMenu();

  m_showAction =
      m_trayMenu->addAction(QIcon::fromTheme("window"), "Show Nixbit");
  connect(m_showAction, &QAction::triggered, this,
          &TrayIconManager::showWindowRequested);

  m_trayMenu->addSeparator();

  m_checkUpdatesAction = m_trayMenu->addAction(QIcon::fromTheme("view-refresh"),
                                               "Check for Updates");
  connect(m_checkUpdatesAction, &QAction::triggered, this,
          &TrayIconManager::checkForUpdatesRequested);

  m_ignoreUpdateAction =
      m_trayMenu->addAction(QIcon::fromTheme("download"), "Ignore Updates");
  m_ignoreUpdateAction->setEnabled(false);
  connect(m_ignoreUpdateAction, &QAction::triggered, this,
          &TrayIconManager::ignoreUpdateRequested);

  m_trayMenu->addSeparator();

  m_quitAction =
      m_trayMenu->addAction(QIcon::fromTheme("application-exit"), "Quit");
  connect(m_quitAction, &QAction::triggered, this,
          &TrayIconManager::quitRequested);

  // Create tray icon
  m_trayIcon = new QSystemTrayIcon(this);
  m_trayIcon->setContextMenu(m_trayMenu);

  connect(m_trayIcon, &QSystemTrayIcon::activated, this,
          &TrayIconManager::onActivated);
  connect(m_trayIcon, &QSystemTrayIcon::messageClicked, this,
          &TrayIconManager::showWindowRequested);

  // Set initial icon
  updateIcon();
}

void TrayIconManager::updateIcon() {
  const int commitsBehind = m_currentCommitsBehind;
  QIcon icon;

  QString debugSuffix = m_debugMode ? " (Debug Mode)" : "";

  if (!m_activity.isEmpty()) {
    icon = m_activityFrames.at(m_activityFrame);
    m_trayIcon->setToolTip(
        QString("Nixbit - %1%2").arg(m_activity, debugSuffix));
  } else if (commitsBehind > 0) {
    // Create icon showing updates available (orange badge with count)
    qDebug() << "Creating update available icon for" << commitsBehind
             << "commits";
    icon = createUpdateAvailableIcon(commitsBehind);
    m_trayIcon->setToolTip(QString("Nixbit - %1 update(s) available%2")
                               .arg(commitsBehind)
                               .arg(debugSuffix));

  } else if (commitsBehind == 0 &&
             m_systemStatus == SystemUpdateStatus::State::NeedsActivation) {
    // The repository is current, but the running system was built from
    // another commit (orange badge with an upward arrow)
    icon = createStatusIcon(snowflakePrimaryColor(), snowflakeSecondaryColor(),
                            QColor(0xf6, 0x74, 0x00), Badge::Pending);
    m_trayIcon->setToolTip(
        QString("Nixbit - Switch needed to activate the latest configuration%1")
            .arg(debugSuffix));
  } else if (commitsBehind == 0 &&
             m_systemStatus == SystemUpdateStatus::State::Current) {
    // Create icon showing system is up to date (green check badge)
    icon = createUpToDateIcon();
    m_trayIcon->setToolTip(
        QString("Nixbit - System up to date%1").arg(debugSuffix));
  } else if (commitsBehind == 0) {
    // Without a configuration revision only the repository can be checked
    // (muted check badge)
    icon = createStatusIcon(snowflakePrimaryColor(), snowflakeSecondaryColor(),
                            QColor(0x7f, 0x8c, 0x8d), Badge::Check);
    m_trayIcon->setToolTip(
        QString("Nixbit - Repository up to date (running system not "
                "verified)%1")
            .arg(debugSuffix));
  } else {
    // Unknown state or error
    qDebug() << "Creating default/unknown icon";
    icon = createDefaultIcon();
    m_trayIcon->setToolTip(
        QString("Nixbit - Repository status unknown%1").arg(debugSuffix));
  }

  // Show which commits the status is based on
  if (m_activity.isEmpty()) {
    QStringList revisions;
    if (!m_latestRevision.isEmpty()) {
      revisions << QString("Latest commit: %1").arg(m_latestRevision);
    }
    if (!m_runningRevision.isEmpty()) {
      revisions << QString("Running: %1").arg(m_runningRevision);
    }
    if (!revisions.isEmpty()) {
      m_trayIcon->setToolTip(m_trayIcon->toolTip() + "\n" +
                             revisions.join(QStringLiteral(" · ")));
    }
  }

  if (m_ignoreUpdateAction) {
    m_ignoreUpdateAction->setEnabled(commitsBehind > 0);
  }

  qDebug() << "Setting tray icon, isNull:" << icon.isNull();
  m_trayIcon->setIcon(icon);
}

QIcon TrayIconManager::createDefaultIcon() {
  // Gray snowflake without badge, the repository status is unknown
  const QColor primary =
      m_debugMode ? QColor(0x8e, 0x6a, 0x9a) : QColor(0x7f, 0x8c, 0x8d);
  const QColor secondary =
      m_debugMode ? QColor(0xc9, 0xb3, 0xd1) : QColor(0xbd, 0xc3, 0xc7);
  return createStatusIcon(primary, secondary, QColor(), Badge::None);
}

QIcon TrayIconManager::createUpToDateIcon() {
  return createStatusIcon(snowflakePrimaryColor(), snowflakeSecondaryColor(),
                          QColor(0x27, 0xae, 0x60), Badge::Check);
}

QIcon TrayIconManager::createUpdateAvailableIcon(int count) {
  return createStatusIcon(snowflakePrimaryColor(), snowflakeSecondaryColor(),
                          QColor(0xf6, 0x74, 0x00), Badge::Count, count);
}

QColor TrayIconManager::snowflakePrimaryColor() const {
  // Purple tones in debug mode to tell both instances apart
  return m_debugMode ? QColor(0x8e, 0x44, 0xad) : QColor(0x52, 0x77, 0xc3);
}

QColor TrayIconManager::snowflakeSecondaryColor() const {
  return m_debugMode ? QColor(0xc3, 0x9b, 0xd3) : QColor(0x7e, 0xba, 0xe4);
}

QIcon TrayIconManager::createStatusIcon(const QColor &primary,
                                        const QColor &secondary,
                                        const QColor &badgeColor, Badge badge,
                                        int count, int angle) {
  // One lambda of the Nix snowflake, normalized to a radius of 1
  static const QPolygonF lambda = {
      {-0.390, 0.022}, {0.097, 0.866},  {-0.126, 0.868},
      {-0.257, 0.641}, {-0.387, 0.867}, {-0.499, 0.867},
      {-0.556, 0.768}, {-0.369, 0.448}, {-0.502, 0.217}};
  QPainterPath lambdaPath;
  lambdaPath.addPolygon(lambda);
  lambdaPath.closeSubpath();

  QIcon icon;
  // Render every size separately, so the icon stays crisp in the panel
  for (const int size : {16, 22, 24, 32, 48, 64, 128}) {
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const qreal half = size / 2.0;
    for (int i = 0; i < 6; ++i) {
      QTransform transform;
      transform.translate(half, half);
      transform.rotate(60 * i);
      transform.scale(half, half);
      painter.fillPath(transform.map(lambdaPath),
                       i % 2 == 0 ? primary : secondary);
    }

    if (badge != Badge::None) {
      const qreal radius = size * 0.3;
      const QPointF center(size - radius, size - radius);

      // Cut out a gap around the badge, so it stands out from the snowflake
      const qreal gap = qMax(1.0, size * 0.06);
      painter.setCompositionMode(QPainter::CompositionMode_Clear);
      painter.setBrush(Qt::black);
      painter.drawEllipse(center, radius + gap, radius + gap);
      painter.setCompositionMode(QPainter::CompositionMode_SourceOver);

      painter.setBrush(badgeColor);
      painter.drawEllipse(center, radius, radius);

      if (badge == Badge::Check) {
        QPainterPath checkPath;
        checkPath.moveTo(center + QPointF(-0.45, 0.02) * radius);
        checkPath.lineTo(center + QPointF(-0.12, 0.35) * radius);
        checkPath.lineTo(center + QPointF(0.45, -0.3) * radius);
        painter.setPen(QPen(Qt::white, qMax(1.5, radius * 0.3), Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(checkPath);
      } else if (badge == Badge::Busy) {
        painter.setPen(QPen(Qt::white, qMax(1.2, radius * 0.25), Qt::SolidLine,
                            Qt::RoundCap));
        painter.setBrush(Qt::NoBrush);
        const qreal arcRadius = radius * 0.55;
        painter.drawArc(QRectF(center.x() - arcRadius, center.y() - arcRadius,
                               arcRadius * 2, arcRadius * 2),
                        -angle * 16, 270 * 16);
      } else if (badge == Badge::Pending) {
        // Upward arrow, drawn like the check mark
        QPainterPath arrowPath;
        arrowPath.moveTo(center + QPointF(0, 0.48) * radius);
        arrowPath.lineTo(center + QPointF(0, -0.42) * radius);
        arrowPath.moveTo(center + QPointF(-0.4, -0.04) * radius);
        arrowPath.lineTo(center + QPointF(0, -0.44) * radius);
        arrowPath.lineTo(center + QPointF(0.4, -0.04) * radius);
        painter.setPen(QPen(Qt::white, qMax(1.5, radius * 0.3), Qt::SolidLine,
                            Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(arrowPath);
      } else {
        // The exact number is shown in the tooltip
        const QString text = count > 9 ? "9+" : QString::number(count);
        QFont font = painter.font();
        font.setBold(true);
        font.setPixelSize(
            qMax(6, qRound(radius * (text.length() > 1 ? 0.95 : 1.4))));
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRectF(center.x() - radius, center.y() - radius,
                                radius * 2, radius * 2),
                         Qt::AlignCenter, text);
      }
    }

    painter.end();
    icon.addPixmap(pixmap);
  }

  return icon;
}

void TrayIconManager::setCommitsBehind(int count) {
  qDebug() << "TrayIconManager::setCommitsBehind called with count:" << count
           << "current:" << m_currentCommitsBehind;
  if (count != m_currentCommitsBehind) {
    // Keep tracking updates while the activity badge takes precedence.
    if (count > m_currentCommitsBehind && m_currentCommitsBehind >= 0) {
      m_trayIcon->showMessage(
          "Nixbit - Updates Available",
          QString("%1 new commit(s) available from origin").arg(count),
          QSystemTrayIcon::Information, 5000);
    }
    m_currentCommitsBehind = count;
    updateIcon();
  } else {
    qDebug() << "Icon update skipped - count unchanged";
  }
}

void TrayIconManager::setActivity(const QString &activity, bool animated) {
  const bool shouldAnimate = !activity.isEmpty() && animated;
  if (activity == m_activity && shouldAnimate == m_activityTimer.isActive()) {
    return;
  }

  if (!activity.isEmpty() && m_activityFrames.isEmpty()) {
    // Cache the frames so animation only changes the icon, not its rendering.
    for (int angle = 0; angle < 360; angle += 30) {
      m_activityFrames.append(
          createStatusIcon(snowflakePrimaryColor(), snowflakeSecondaryColor(),
                           QColor(0x29, 0x80, 0xb9), Badge::Busy, 0, angle));
    }
  }

  m_activity = activity;
  if (shouldAnimate) {
    m_activityTimer.start();
  } else {
    m_activityTimer.stop();
    m_activityFrame = 0;
  }
  updateIcon();
}

void TrayIconManager::setSystemStatus(SystemUpdateStatus::State state,
                                      const QString &runningRevision,
                                      const QString &latestRevision) {
  if (m_systemStatus != state || m_runningRevision != runningRevision ||
      m_latestRevision != latestRevision) {
    m_systemStatus = state;
    m_runningRevision = runningRevision;
    m_latestRevision = latestRevision;
    updateIcon();
  }
}

void TrayIconManager::show() {
  if (m_trayIcon) {
    m_trayIcon->show();
  }
}

void TrayIconManager::hide() {
  if (m_trayIcon) {
    m_trayIcon->hide();
  }
}

void TrayIconManager::showNotification(const QString &title,
                                       const QString &message) {
  // QSystemTrayIcon::showMessage() doesn't offer actions, so talk to the
  // notification server directly
  QDBusMessage call =
      QDBusMessage::createMethodCall(notificationsService, notificationsPath,
                                     notificationsInterface, "Notify");
  QVariantMap hints;
  hints.insert("desktop-entry", "nixbit");
  hints.insert("urgency", QVariant::fromValue(uchar(1)));
  // "default" is triggered by clicking the notification itself
  const QStringList actions{"default", "Show Nixbit", "show", "Show Nixbit"};
  // Expire timeout 0 keeps the notification until it is closed
  call << QStringLiteral("Nixbit") << m_notificationId
       << QStringLiteral("dialog-password") << title << message << actions
       << hints << 0;

  auto *watcher = new QDBusPendingCallWatcher(
      QDBusConnection::sessionBus().asyncCall(call), this);
  connect(watcher, &QDBusPendingCallWatcher::finished, this,
          [this, watcher, title, message]() {
            const QDBusPendingReply<uint> reply = *watcher;
            if (reply.isError()) {
              qWarning() << "Failed to show notification:"
                         << reply.error().message();
              if (m_trayIcon) {
                m_trayIcon->showMessage(title, message,
                                        QSystemTrayIcon::Warning, 30000);
              }
            } else {
              m_notificationId = reply.value();
            }
            watcher->deleteLater();
          });
}

void TrayIconManager::closeNotification() {
  if (m_notificationId == 0) {
    return;
  }
  QDBusMessage call = QDBusMessage::createMethodCall(
      notificationsService, notificationsPath, notificationsInterface,
      "CloseNotification");
  call << m_notificationId;
  QDBusConnection::sessionBus().asyncCall(call);
  m_notificationId = 0;
}

void TrayIconManager::onNotificationActionInvoked(uint id,
                                                  const QString &action) {
  if (id == 0 || id != m_notificationId) {
    return;
  }
  if (action == "default" || action == "show") {
    emit showWindowRequested();
  }
  closeNotification();
}

void TrayIconManager::onNotificationActivationToken(uint id,
                                                    const QString &token) {
  // Sent right before ActionInvoked, Qt uses this token on Wayland to allow
  // the window to be activated
  if (id != 0 && id == m_notificationId && !token.isEmpty()) {
    qputenv("XDG_ACTIVATION_TOKEN", token.toUtf8());
  }
}

void TrayIconManager::onNotificationClosed(uint id, uint reason) {
  Q_UNUSED(reason);
  if (id != 0 && id == m_notificationId) {
    m_notificationId = 0;
  }
}

void TrayIconManager::onActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger ||
      reason == QSystemTrayIcon::DoubleClick) {
    emit toggleWindowRequested();
  }
}
