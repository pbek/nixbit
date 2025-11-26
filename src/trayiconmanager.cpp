#include "trayiconmanager.h"
#include <QApplication>
#include <QDebug>
#include <QFont>
#include <QIcon>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>

TrayIconManager::TrayIconManager(QObject *parent)
    : QObject(parent), m_trayIcon(nullptr), m_trayMenu(nullptr),
      m_currentCommitsBehind(0) {
  createTrayIcon();
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

  // Set initial icon
  updateIcon(0);
}

void TrayIconManager::updateIcon(int commitsBehind) {
  qDebug() << "TrayIconManager::updateIcon called with commitsBehind:"
           << commitsBehind;
  QPixmap pixmap;

  if (commitsBehind > 0) {
    // Create icon showing updates available (orange/yellow theme)
    qDebug() << "Creating update available icon for" << commitsBehind
             << "commits";
    pixmap = createUpdateAvailableIcon(commitsBehind);
    m_trayIcon->setToolTip(
        QString("Nixbit - %1 update(s) available").arg(commitsBehind));

    // Show a notification if commits behind increased
    if (commitsBehind > m_currentCommitsBehind && m_currentCommitsBehind >= 0) {
      qDebug() << "Showing notification for increased commits";
      m_trayIcon->showMessage(
          "Nixbit - Updates Available",
          QString("%1 new commit(s) available from origin").arg(commitsBehind),
          QSystemTrayIcon::Information, 5000);
    }
  } else if (commitsBehind == 0) {
    // Create icon showing system is up to date (green theme)
    qDebug() << "Creating up-to-date icon";
    pixmap = createUpToDateIcon();
    m_trayIcon->setToolTip("Nixbit - System up to date");
  } else {
    // Unknown state or error
    qDebug() << "Creating default/unknown icon";
    pixmap = createDefaultIcon();
    m_trayIcon->setToolTip("Nixbit - Repository status unknown");
  }

  qDebug() << "Setting tray icon, pixmap size:" << pixmap.size()
           << "isNull:" << pixmap.isNull();
  m_trayIcon->setIcon(QIcon(pixmap));
  m_currentCommitsBehind = commitsBehind;
  qDebug() << "Icon updated, m_currentCommitsBehind now:"
           << m_currentCommitsBehind;
}

QPixmap TrayIconManager::createDefaultIcon() {
  // Create a neutral gray/blue icon (like system-software-update)
  QPixmap pixmap(48, 48);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw a downward arrow in a circle (update symbol)
  QPen pen(QColor(100, 100, 120), 3);
  painter.setPen(pen);
  painter.setBrush(QBrush(QColor(220, 220, 230)));

  // Circle
  painter.drawEllipse(4, 4, 40, 40);

  // Arrow pointing down
  painter.setBrush(QBrush(QColor(80, 80, 100)));
  painter.setPen(Qt::NoPen);

  // Arrow shaft
  painter.drawRect(20, 13, 8, 15);

  // Arrow head
  QPolygon arrowHead;
  arrowHead << QPoint(24, 34) << QPoint(15, 26) << QPoint(33, 26);
  painter.drawPolygon(arrowHead);

  return pixmap;
}

QPixmap TrayIconManager::createUpToDateIcon() {
  // Create a green checkmark icon (system is up to date)
  QPixmap pixmap(48, 48);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw a green circle background
  QPen pen(QColor(40, 150, 60), 2);
  painter.setPen(pen);
  painter.setBrush(QBrush(QColor(100, 200, 120)));
  painter.drawEllipse(4, 4, 40, 40);

  // Draw checkmark
  painter.setPen(
      QPen(Qt::white, 4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
  painter.setBrush(Qt::NoBrush);

  QPainterPath checkPath;
  checkPath.moveTo(14, 24);
  checkPath.lineTo(21, 31);
  checkPath.lineTo(34, 17);
  painter.drawPath(checkPath);

  return pixmap;
}

QPixmap TrayIconManager::createUpdateAvailableIcon(int count) {
  // Create an orange/amber icon with update count (updates available)
  QPixmap pixmap(48, 48);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);

  // Draw orange circle background
  QPen pen(QColor(220, 130, 20), 2);
  painter.setPen(pen);
  painter.setBrush(QBrush(QColor(255, 160, 50)));
  painter.drawEllipse(4, 4, 40, 40);

  // Draw downward arrow (update symbol)
  painter.setBrush(QBrush(Qt::white));
  painter.setPen(Qt::NoPen);

  // Arrow shaft
  painter.drawRect(20, 13, 8, 15);

  // Arrow head
  QPolygon arrowHead;
  arrowHead << QPoint(24, 34) << QPoint(15, 26) << QPoint(33, 26);
  painter.drawPolygon(arrowHead);

  // Draw count badge if more than 1 update
  if (count > 1) {
    // Red notification badge in top-right corner
    painter.setBrush(QBrush(QColor(220, 50, 50)));
    painter.setPen(QPen(Qt::white, 1.5));
    painter.drawEllipse(28, 0, 20, 20);

    // Draw count text
    painter.setPen(Qt::white);
    QFont font = painter.font();
    font.setPixelSize(count > 9 ? 10 : 12);
    font.setBold(true);
    painter.setFont(font);

    QString text = count > 99 ? "99+" : QString::number(count);
    painter.drawText(QRect(28, 0, 20, 20), Qt::AlignCenter, text);
  }

  return pixmap;
}

void TrayIconManager::setCommitsBehind(int count) {
  qDebug() << "TrayIconManager::setCommitsBehind called with count:" << count
           << "current:" << m_currentCommitsBehind;
  if (count != m_currentCommitsBehind) {
    qDebug() << "Updating icon from" << m_currentCommitsBehind << "to" << count;
    updateIcon(count);
  } else {
    qDebug() << "Icon update skipped - count unchanged";
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

void TrayIconManager::onActivated(QSystemTrayIcon::ActivationReason reason) {
  if (reason == QSystemTrayIcon::Trigger ||
      reason == QSystemTrayIcon::DoubleClick) {
    emit toggleWindowRequested();
  }
}
