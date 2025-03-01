/**
 * UGENE - Integrated Bioinformatics Tools.
 * Copyright (C) 2008-2022 UniPro <ugene@unipro.ru>
 * http://ugene.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 */

#include "Notification.h"

#include <QApplication>
#include <QMenu>
#include <QStatusBar>
#include <QTextBrowser>
#include <QTime>

#include <U2Core/AppContext.h>
#include <U2Core/QObjectScopedPointer.h>
#include <U2Core/U2SafePoints.h>

#include "MainWindow.h"

namespace U2 {

Notification::Notification(NotificationStack* _stack, const QString& _text, const NotificationType& _type, QAction* _action)
    : action(_action), stack(_stack), text(_text), type(_type) {
    SAFE_POINT(stack != nullptr, "Stack must be defined", );

    setFixedWidth(TT_WIDTH);
    setMinimumHeight(TT_HEIGHT);

    timestamp = QDateTime::currentMSecsSinceEpoch();

    setFrameStyle(QFrame::StyledPanel);
    closeButton = new QLabel(this);
    QBoxLayout* h = new QHBoxLayout(this);
    setLayout(h);

    updateDisplayText();

    updateStyle(false);
    updateCloseButtonStyle(false);

    setWindowFlags(Qt::Window | Qt::FramelessWindowHint | Qt::Tool);

    closeButton->installEventFilter(this);
    h->addStretch();
    h->addWidget(closeButton);
    closeButton->hide();
    setMouseTracking(true);

    closeButton->setAttribute(Qt::WA_Hover);
    closeButton->setFixedSize(16, 16);
}

void Notification::updateDisplayText() {
    QFontMetrics metrics(font(), this);

    QString counterText = counter == 0 ? "" : "(" + QString::number(counter + 1) + ") ";
    int counterTextWidth = counter == 0 ? 0 : metrics.width(counterText);

    QString timestampText = "[" + QDateTime::fromMSecsSinceEpoch(timestamp).time().toString() + "] ";
    QString displayText = timestampText + text;

    setText(metrics.elidedText(timestampText + text, Qt::ElideRight, width() - 50 - counterTextWidth) + counterText);
    setToolTip(displayText);
}

void Notification::updateStyle(bool isHovered) {
    QString bgColor;
    QString img;
    QString fontColor;
    QString border;
    switch (type) {
        case Info_Not:
            bgColor = "background-color: #BDE5F8;";
            fontColor = "color: #00529B;";
            img = "background-image: url(':core/images/info_notification.png');";
            break;
        case Error_Not:
            bgColor = "background-color: #FFBABA;";
            fontColor = "color: #D8000C;";
            img = "background-image: url(':core/images/error_notification.png');";
            break;
        case Report_Not:
            bgColor = "background-color: #BDE5F8;";
            fontColor = "color: #00529B;";
            img = "background-image: url(':core/images/info_notification.png');";
            break;
        case Warning_Not:
            bgColor = "background-color: #FCF8E3;";
            fontColor = "color: #C09853;";
            img = "background-image: url(':core/images/warning_notification.png');";
            break;
        default:
            assert(0);
    }

    if (isHovered) {
        border = "border: 2px solid;";
    } else {
        border = "border: 1px solid;";
    }
    QString css;

    css.append(border);
    css.append("padding: 2px 2px 2px 20px;");
    css.append("background-repeat: no-repeat;");
    css.append("background-position: left center;");
    css.append(fontColor);
    css.append(bgColor);
    css.append(img);

    setStyleSheet(css);
}

void Notification::updateCloseButtonStyle(bool isHovered) {
    QString css;
    QString background;
    if (isHovered) {
        css = "border: 1px solid;";
        background = "background-color: #C0C0C0;";
    } else {
        css = "border: none;";
        background = "background-color: transparent;";
    }

    css.append("border-radius: 3px;");
    css.append("background-position: center center;");
    css.append("padding: 2px 2px 2px 2px;");
    css.append(background);
    css.append("background-image: url(':core/images/close.png');");
    closeButton->setStyleSheet(css);
}

bool Notification::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) {
        if (auto helpEvent = dynamic_cast<QHelpEvent*>(e)) {
            QToolTip::showText(helpEvent->globalPos(), QString(text));
            return true;
        }
    }
    if (e->type() == QEvent::HoverEnter) {
        updateStyle(true);
    } else if (e->type() == QEvent::HoverLeave) {
        updateStyle(false);
    }
    return QLabel::event(e);
}

void Notification::mousePressEvent(QMouseEvent* ev) {
    if (ev->button() != Qt::LeftButton) {
        return;
    }
    if (isOnScreen()) {
        hideFloatingOnScreen();
    }
    if (action) {
        action->trigger();
        return;
    }

    QObjectScopedPointer<QDialog> dlg = new QDialog(AppContext::getMainWindow()->getQMainWindow());
    dlg->setObjectName("NotificationDialog");
    QVBoxLayout vLayout;
    QHBoxLayout hLayout;
    QPushButton ok;
    QCheckBox isDelete;

    ok.setText(tr("OK"));
    isDelete.setText(tr("Remove notification after closing"));
    isDelete.setChecked(true);
    connect(&ok, SIGNAL(clicked()), dlg.data(), SLOT(accept()));
    hLayout.addWidget(&isDelete);
    hLayout.addWidget(&ok);

    dlg->setLayout(&vLayout);
    QTextBrowser txtEdit;
    txtEdit.setOpenExternalLinks(true);
    txtEdit.setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    dlg->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    txtEdit.setReadOnly(true);
    txtEdit.setText(text);
    vLayout.addWidget(&txtEdit);
    vLayout.addLayout(&hLayout);

    dlg->setWindowTitle(tr("Detailed message"));

    stack->setFixed(true);
    int dialogResult = dlg->exec();
    stack->setFixed(false);
    CHECK(!dlg.isNull(), );

    if (dialogResult == QDialog::Accepted) {
        if (isDelete.isChecked()) {
            emit si_deleteRequested();
        }
    }
}

bool Notification::eventFilter(QObject*, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {  // Close button clicked.
        if (auto mouseEvent = dynamic_cast<QMouseEvent*>(event)) {
            if (mouseEvent->button() == Qt::LeftButton) {
                emit si_deleteRequested();
                return true;
            }
        }
    } else if (event->type() == QEvent::HoverEnter) {
        updateCloseButtonStyle(true);
    } else if (event->type() == QEvent::HoverLeave) {
        updateCloseButtonStyle(false);
    }
    return false;
}

void Notification::showFloatingOnScreen() {
    onScreenStartTimeMillis = QDateTime::currentMSecsSinceEpoch();
    if (!onScreenTimer.isActive()) {
        onScreenTimer.setInterval(20);
        connect(&onScreenTimer, SIGNAL(timeout()), SLOT(sl_updateNotificationState()));
        onScreenTimer.start();
    }
}

void Notification::switchEmbeddedVisualState() {
    closeButton->show();
    setAttribute(Qt::WA_Hover);
}

void Notification::hideFloatingOnScreen() {
    hide();
    lastOnScreenHeight = TT_HEIGHT;
    onScreenTimer.stop();
    onScreenStartTimeMillis = 0;
    emit si_notificationHideEvent();
}

void Notification::sl_updateNotificationState() {
    qint64 onScreenTimeMillis = QDateTime::currentMSecsSinceEpoch() - onScreenStartTimeMillis;
    if (onScreenTimeMillis >= ON_SCREEN_TIMEOUT_MILLIS) {
        hideFloatingOnScreen();
        return;
    }
    if (isOnScreen()) {
        QRect rect = geometry();
        if (rect.height() != lastOnScreenHeight) {
            lastOnScreenHeight = rect.height();
            stack->updateOnScreenNotificationPositions();
        }
    }
}

const QString& Notification::getText() const {
    return text;
}

const NotificationType& Notification::getType() const {
    return type;
}

QAction* Notification::getAction() const {
    return action;
}

void Notification::incrementCounter() {
    counter++;
    timestamp = QDateTime::currentMSecsSinceEpoch();

    updateDisplayText();

    // Re-set on-screen time timeout.
    onScreenStartTimeMillis = QDateTime::currentMSecsSinceEpoch();
}

qint64 Notification::getOnScreenStartTimeMillis() const {
    return onScreenStartTimeMillis;
}

bool Notification::isOnScreen() const {
    return onScreenTimer.isActive();
}

NotificationStack::NotificationStack(QWidget* _parentWidget)
    : QObject(_parentWidget), parentWidget(_parentWidget) {
    SAFE_POINT(parentWidget, "Parent widget is null!", );
    notificationWidget = new NotificationWidget(parentWidget);
    parentWidget->installEventFilter(this);
}

NotificationStack::~NotificationStack() {
    foreach (Notification* notification, notifications) {
        delete notification;
    }
    delete notificationWidget;
}

bool NotificationStack::hasError() const {
    foreach (Notification* n, notifications) {
        if (n->getType() == Error_Not) {
            return true;
        }
    }
    return false;
}

void NotificationStack::addNotification(const QString& text, const NotificationType& type, QAction* action) {
    auto stack = AppContext::getMainWindow()->getNotificationStack();
    if (stack != nullptr) {
        stack->add(text, type, action);
    }
}

void NotificationStack::add(const QString& text, const NotificationType& type, QAction* action) {
    for (Notification* n : qAsConst(notifications)) {
        if (n->isOnScreen() && n->getText() == text && n->getType() == type && n->getAction() == action) {
            n->incrementCounter();
            n->showFloatingOnScreen();
            updateOnScreenNotificationPositions();
            emit si_changed();
            return;
        }
    }

    if (notifications.count() >= MAX_STACK_SIZE) {
        // Replace the oldest notification. If all notifications are on-screen -> select the oldest on-screen one.
        Notification* notificationToRemove = notifications.first();
        if (notificationToRemove->getOnScreenStartTimeMillis() > 0) {
            notificationToRemove = *std::min_element(notifications.begin(), notifications.end(), [](auto n1, auto n2) {
                return n1->getOnScreenStartTimeMillis() < n2->getOnScreenStartTimeMillis();
            });
        }
        emit notificationToRemove->si_deleteRequested();
    }

    auto newNotification = new Notification(this, text, type, action);

    connect(newNotification, SIGNAL(si_deleteRequested()), this, SLOT(sl_onNotificationDeleteRequest()));
    connect(newNotification, SIGNAL(si_notificationHideEvent()), SLOT(sl_onNotificationHidden()));

    notifications.append(newNotification);
    newNotification->showFloatingOnScreen();
    updateOnScreenNotificationPositions();
    emit si_changed();
}

void NotificationStack::updateOnScreenNotificationPositions() {
    // Compact all notifications.
    // Move the most recent notifications on top.
    // Make the new notifications visible.
    QPoint bottomRight = getStackBottomRightPoint();
    int yBottomOffset = 0;
    for (Notification* notification : qAsConst(notifications)) {
        if (notification->isOnScreen()) {
            if (!notification->isVisible()) {
                notification->show();
            }
            notification->raise();  // Ensure that notification window is still on top of the all windows stack.
            int height = qMax(notification->height(), TT_HEIGHT);
            QPoint newTopLeft(bottomRight.x() - TT_WIDTH, bottomRight.y() - yBottomOffset - height);
            if (newTopLeft != notification->pos()) {
                notification->move(newTopLeft);
            }
            yBottomOffset += height;
        }
    }
}

void NotificationStack::sl_onNotificationHidden() {
    auto hiddenNotification = qobject_cast<Notification*>(sender());
    SAFE_POINT(hiddenNotification != nullptr, "Sender is not a Notification!", );

    // TODO: on-screen notifications must always be inside of the notification widget from the very beginning.
    moveToNotificationWidget(hiddenNotification);
    updateOnScreenNotificationPositions();
}

void NotificationStack::moveToNotificationWidget(Notification* t) {
    t->switchEmbeddedVisualState();
    t->show();
    t->setParent(notificationWidget);
    notificationWidget->addNotification(t);
}

void NotificationStack::sl_onNotificationDeleteRequest() {
    auto notification = qobject_cast<Notification*>(sender());
    SAFE_POINT(notification, "Sender is not Notification", );

    notifications.removeOne(notification);
    disconnect(notification);

    if (notification->isOnScreen()) {
        notification->hide();
        notification->deleteLater();
        updateOnScreenNotificationPositions();
    } else {
        notificationWidget->removeNotification(notification);  // Calls "delete notification" for children.
    }
    emit si_changed();
}

int NotificationStack::count() const {
    return notifications.count();
}

void NotificationStack::showStack() {
    QPoint pos = getStackBottomRightPoint();

    notificationWidget->move(pos.x() - notificationWidget->width(), pos.y() - notificationWidget->height());
    notificationWidget->show();
    notificationWidget->setWindowState(Qt::WindowActive);
}

void NotificationStack::setFixed(bool val) {
    notificationWidget->setFixed(val);
}

QPoint NotificationStack::getStackBottomRightPoint() const {
    QPoint parentTopLeft = parentWidget->mapToGlobal(QPoint(0, 0));
    QPoint parentBottomRight = parentTopLeft + QPoint(parentWidget->width(), parentWidget->height());

    // Offset from the bottom right point if the parent widget: stack is shown inside with no overlap with borders/status bar.
    QPoint bottomRightOffset(10, 50);
    return parentBottomRight - bottomRightOffset;
}

bool NotificationStack::eventFilter(QObject* target, QEvent* event) {
    if (target == parentWidget) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            QTimer::singleShot(100, this, [this]() { updateOnScreenNotificationPositions(); });
        }
    }
    return false;
}

}  // namespace U2
