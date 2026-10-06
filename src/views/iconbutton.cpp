// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "iconbutton.h"
#include "utils.h"

//qt
#include <QDebug>
#include <QLoggingCategory>
#include <QPainter>
#include <DGuiApplicationHelper>

Q_DECLARE_LOGGING_CATEGORY(views)
IconButton::IconButton(QWidget *parent)
    : DIconButton(parent)
{
    // qCDebug(views) << "Enter IconButton::IconButton";
    Utils::set_Object_Name(this);
}

void IconButton::keyPressEvent(QKeyEvent *event)
{
    // qCDebug(views) << "IconButton::keyPressEvent() entered, key:" << event->key();

    // 不处理向右的事件
    switch (event->key()) {
    case Qt::Key_Right:
    case Qt::Key_Up:
    case Qt::Key_Down:
        qCDebug(views)  << "Ignoring up/down key event";
        event->ignore();
        break;
    case Qt::Key_Left:
        qCDebug(views)  << "Processing left key, emitting preFocus";
        emit preFocus();
        break;
    default:
        // qCDebug(views) << "Default key handling";
        DIconButton::keyPressEvent(event);
        break;
    }
}

void IconButton::focusOutEvent(QFocusEvent *event)
{
    // qCDebug(views) << event->reason() << "IconButton" << this;
    emit focusOut(event->reason());
    DIconButton::focusOutEvent(event);
}

void IconButton::setBackArrow()
{
    if (!m_backArrow) {
        m_backArrow = true;
        connect(Dtk::Gui::DGuiApplicationHelper::instance(),
                &Dtk::Gui::DGuiApplicationHelper::themeTypeChanged,
                this, [this] { updateBackArrow(); });
    }
    updateBackArrow();
}

void IconButton::changeEvent(QEvent *event)
{
    DIconButton::changeEvent(event);
    if (m_backArrow && (event->type() == QEvent::PaletteChange
                       || event->type() == QEvent::StyleChange))
        updateBackArrow();
}

void IconButton::updateBackArrow()
{
    // The DTK standard arrow uses a fixed dark stroke in some styles.
    // Render both enabled and disabled variants for the current theme.
    const bool dark = Dtk::Gui::DGuiApplicationHelper::instance()->themeType()
        == Dtk::Gui::DGuiApplicationHelper::DarkType;
    QIcon arrow;
    for (auto mode : {QIcon::Normal, QIcon::Disabled}) {
        const qreal ratio = devicePixelRatioF();
        QPixmap pixmap(qRound(20 * ratio), qRound(20 * ratio));
        pixmap.setDevicePixelRatio(ratio);
        pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        QColor color(dark ? "#C0C6D4" : "#536076");
        if (mode == QIcon::Disabled) color.setAlphaF(0.4);
        painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPolyline(QPolygonF({QPointF(12, 5), QPointF(7, 10), QPointF(12, 15)}));
        painter.end();
        arrow.addPixmap(pixmap, mode);
    }
    setIcon(arrow);
}
