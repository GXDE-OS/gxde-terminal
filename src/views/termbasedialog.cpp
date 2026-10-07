// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "termbasedialog.h"

//dtk
#include <DGuiApplicationHelper>

//qt
#include <QLabel>
#include <QDebug>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(views)

DGUI_USE_NAMESPACE

/*******************************************************************************
 1. @函数:    palrtteTransparency
 2. @作者:    ut000610 daizhengwen
 3. @日期:    2020-08-11
 4. @说明:    调色板透明度
*******************************************************************************/
static void palrtteTransparency(QWidget *widget, qint8 alphaFloat)
{
    qCDebug(views) << "Enter palrtteTransparency";
    QPalette palette = widget->palette();
    QColor color = DGuiApplicationHelper::adjustColor(palette.color(QPalette::BrightText), 0, 0, 0, 0, 0, 0, alphaFloat);
    palette.setColor(QPalette::WindowText, color);
    widget->setPalette(palette);
}

//fix bug 23481 主菜单切换主题，弹框字体颜色没有随主题及时变换
/*******************************************************************************
 1. @函数:    paintEvent
 2. @作者:    ut000610 daizhengwen
 3. @日期:    2020-08-11
 4. @说明:    绘画事件
*******************************************************************************/
void QWidget::paintEvent(QPaintEvent *e)
{
    // qCDebug(views) << "Enter TermBaseDialog::paintEvent";
    Q_UNUSED(e)

    if (strcmp(this->metaObject()->className(), "Dtk::Widget::DDialog") != 0) {
        // qCDebug(views) << "Not DDialog, skip painting";
        return;
    }

    QLabel *titleLabel = this->findChild<QLabel *>("TitleLabel");
    QLabel *messageLabel = this->findChild<QLabel *>("MessageLabel");

    if (titleLabel != nullptr) {
        // qCDebug(views) << "Adjusting titleLabel transparency";
        palrtteTransparency(titleLabel, -10);
    }

    if (messageLabel != nullptr) {
        // qCDebug(views) << "Adjusting messageLabel transparency";
        palrtteTransparency(messageLabel, -30);
    }

}

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QIcon>
#include <DStyle>
#include <DBlurEffectWidget>
#include <QPainter>
#include <QResizeEvent>

TermCloseDialog::TermCloseDialog(const QString &title, const QString &message,
                                 const QString &cancelText, const QString &closeText,
                                 QWidget *parent)
    : DAbstractDialog(false, parent)
{
    setObjectName(QStringLiteral("TermCloseDialog"));
    setWindowTitle(title);
    setWindowIcon(QIcon(QStringLiteral(":/other/gxde-window/dialog_icon.svg")));
    setFixedWidth(380);
    setAttribute(Qt::WA_TranslucentBackground);
    auto *blur = new Dtk::Widget::DBlurEffectWidget(this);
    blur->setObjectName(QStringLiteral("CloseDialogBlur"));
    blur->setAttribute(Qt::WA_TransparentForMouseEvents);
    blur->setBlendMode(Dtk::Widget::DBlurEffectWidget::BehindWindowBlend);
    blur->setFull(true);
    blur->setBlurEnabled(true);
    blur->setBlurRectXRadius(8);
    blur->setBlurRectYRadius(8);
    blur->setGeometry(rect());
    blur->lower();
    blur->show();
    auto transparentPalette = palette();
    transparentPalette.setColor(QPalette::Window, Qt::transparent);
    setPalette(transparentPalette);
    setAutoFillBackground(false);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto *dismiss = new QPushButton(this);
    dismiss->setObjectName(QStringLiteral("DismissButton"));
    dismiss->setAccessibleName(closeText);
    dismiss->setFixedSize(28, 28);
    // The 40px SVG includes padding around an 8.5px cross.
    dismiss->setIconSize(QSize(40, 40));
    dismiss->setAutoDefault(false);
    Dtk::Widget::DStyle::setFocusRectVisible(dismiss, false);
    connect(dismiss, &QPushButton::clicked, this, &QDialog::reject);

    auto *body = new QHBoxLayout;
    body->setContentsMargins(20, 43, 20, 24);
    body->setSpacing(20);
    auto *icon = new QLabel(this);
    icon->setPixmap(windowIcon().pixmap(64, 64));
    icon->setFixedSize(64, 64);
    body->addWidget(icon, 0, Qt::AlignVCenter);
    auto *text = new QVBoxLayout;
    text->setSpacing(3);
    text->setContentsMargins(0, 7, 0, 0);
    auto *heading = new QLabel(title, this);
    heading->setObjectName(QStringLiteral("CloseHeading"));
    heading->setTextFormat(Qt::PlainText);
    heading->setWordWrap(true);
    auto *description = new QLabel(message, this);
    description->setObjectName(QStringLiteral("CloseDescription"));
    description->setTextFormat(Qt::PlainText);
    description->setWordWrap(true);
    text->addWidget(heading);
    text->addWidget(description);
    text->addStretch();
    body->addLayout(text, 1);
    layout->addLayout(body);

    auto *actions = new QHBoxLayout;
    actions->setSpacing(0);
    auto *cancel = new QPushButton(cancelText, this);
    cancel->setObjectName(QStringLiteral("CancelAction"));
    auto *close = new QPushButton(closeText, this);
    close->setObjectName(QStringLiteral("CloseAction"));
    for (auto *button : {cancel, close}) {
        button->setFixedHeight(28);
        Dtk::Widget::DStyle::setFocusRectVisible(button, false);
        button->setAutoDefault(false);
        actions->addWidget(button, 1);
    }
    // Keep the existing Enter-to-confirm behavior; Escape and × cancel.
    close->setDefault(true);
    // Do not highlight an action before the user hovers or tabs to it.
    setFocusPolicy(Qt::StrongFocus);
    setFocus(Qt::OtherFocusReason);
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    connect(close, &QPushButton::clicked, this, &QDialog::accept);
    layout->addLayout(actions);
    connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::themeTypeChanged,
            this, &TermCloseDialog::updateColors);
    updateColors();
    dismiss->raise();
}

void TermCloseDialog::updateColors()
{
    const bool dark = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType;
    if (auto *blur = findChild<Dtk::Widget::DBlurEffectWidget *>(QStringLiteral("CloseDialogBlur"))) {
        blur->setMaskColor(dark ? QColor(30, 31, 35) : QColor(Qt::white));
        blur->setMaskAlpha(199);
    }
    if (auto *dismiss = findChild<QPushButton *>(QStringLiteral("DismissButton"))) {
        dismiss->setIcon(QIcon(QStringLiteral(":/other/gxde-window/window_close_%1_normal.svg")
                              .arg(dark ? QStringLiteral("dark") : QStringLiteral("light"))));
    }
    // Preserve master's layout and focus treatment with theme-aware colors.
    setStyleSheet(QStringLiteral(
        "QLabel#CloseHeading { color: %1; font-size: 12px; font-weight: bold; background: transparent; }"
        "QLabel#CloseDescription { color: %2; font-size: 12px; background: transparent; }"
        "QPushButton#CancelAction, QPushButton#CloseAction {"
        " background: transparent; border: none; border-top: 1px solid %3;"
        " border-radius: 0; color: %1; font-size: 12px; outline: none; padding: 0 8px; }"
        "QPushButton#CancelAction { border-right: 1px solid %3; border-bottom-left-radius: 5px; }"
        "QPushButton#CloseAction { color: %4; font-weight: bold; border-bottom-right-radius: 5px; }"
        "QPushButton#CancelAction:hover, QPushButton#CloseAction:hover,"
        "QPushButton#CancelAction:focus, QPushButton#CloseAction:focus {"
        " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #8ccfff,stop:1 #4bb8ff);"
        " border-color: #3cabfd; color: white; }"
        "QPushButton#CancelAction:pressed, QPushButton#CloseAction:pressed {"
        " background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #0b8cff,stop:1 #0aa1ff); color: white; }"
        "QPushButton#DismissButton { border: none; border-radius: 4px; padding: 0; background: transparent; }"
        "QPushButton#DismissButton:hover { background: %3; }"
        "QPushButton#DismissButton:pressed { background: rgba(128,128,128,0.25); }")
        .arg(dark ? "#f0f0f0" : "#303030", dark ? "#c4c4c4" : "#444444",
             dark ? "rgba(255,255,255,0.10)" : "rgba(0,0,0,0.10)",
             dark ? "#ff7777" : "#ff5a5a"));
    update();
}

void TermCloseDialog::paintEvent(QPaintEvent *)
{
    // The blur child owns the translucent fill; never cover it with opaque paint.
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool dark = DGuiApplicationHelper::instance()->themeType() == DGuiApplicationHelper::DarkType;
    painter.setPen(dark ? QColor(255, 255, 255, 20) : QColor(0, 0, 0, 26));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}

void TermCloseDialog::resizeEvent(QResizeEvent *event)
{
    DAbstractDialog::resizeEvent(event);
    if (auto *blur = findChild<Dtk::Widget::DBlurEffectWidget *>(QStringLiteral("CloseDialogBlur"))) {
        blur->setGeometry(rect());
        blur->lower();
    }
    if (auto *dismiss = findChild<QPushButton *>(QStringLiteral("DismissButton")))
        dismiss->move(width() - dismiss->width() - 4, 4);
}
