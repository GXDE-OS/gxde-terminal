// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "encodepanel.h"

#include "encodelistview.h"
#include "encodelistmodel.h"
#include "settings.h"
#include "gxderemotestyle.h"

#include <DLog>
#include <QScroller>
#include <QVBoxLayout>

Q_DECLARE_LOGGING_CATEGORY(encodeplugin)

EncodePanel::EncodePanel(QWidget *parent)
    : RightPanel(parent), m_encodeView(new EncodeListView(this))
{
    qCDebug(encodeplugin) << "EncodePanel constructor enter";
    /******** Add by ut001000 renfeixiang 2020-08-14:增加 Begin***************/
    Utils::set_Object_Name(this);
    m_encodeView->setObjectName("EncodeListView");
    /******** Add by ut001000 renfeixiang 2020-08-14:增加 End***************/
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    setFocusProxy(m_encodeView);

    // Let the list follow the panel throughout resizing and slide animations.
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(m_encodeView);
    applyGxdePanelStyle(this);

    connect(m_encodeView, &EncodeListView::focusOut, this, &RightPanel::hideAnim);
    qCDebug(encodeplugin) << "EncodePanel constructor exit";
}

void EncodePanel::show()
{
    qCDebug(encodeplugin) << "EncodePanel show enter";
    this->showAnim();

    qCDebug(encodeplugin) << "EncodePanel show exit";
}

void EncodePanel::updateEncode(QString encode)
{
    qCDebug(encodeplugin) << "updateEncode enter, encode:" << encode;
    m_encodeView->checkEncode(encode);
    qCDebug(encodeplugin) << "updateEncode exit";
}

