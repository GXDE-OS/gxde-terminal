// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "titlebar.h"
#include "utils.h"

#include <DApplication>
#include <DIconButton>

#include <QIcon>
#include <QLabel>
#include <QDebug>
#include <QMouseEvent>
#include <QLoggingCategory>
#include <QAbstractButton>
#include <QPainter>
#include <DGuiApplicationHelper>

Q_DECLARE_LOGGING_CATEGORY(views)

#ifdef DTKWIDGET_CLASS_DSizeMode
#include <DSizeMode>
#endif

namespace {
class GxdeWindowButtonStyle : public QObject
{
public:
    GxdeWindowButtonStyle(QAbstractButton *button, const QString &iconName)
        : QObject(button), m_button(button), m_iconName(iconName)
    {
        button->installEventFilter(this);
        button->setFixedSize(iconName == "tab_add" ? 50 : 40, WIN_TITLE_BAR_HEIGHT);
        button->setCursor(Qt::PointingHandCursor);
        connect(Dtk::Gui::DGuiApplicationHelper::instance(),
                &Dtk::Gui::DGuiApplicationHelper::themeTypeChanged, button,
                [button] { button->update(); });
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object != m_button || event->type() != QEvent::Paint)
            return false;
        const QString name = m_iconName == "max" && m_button->property("isMaximized").toBool()
            ? QStringLiteral("unmax") : m_iconName;
        QString theme = m_button->property("gxdeWindowButtonTheme").toString();
        if (theme.isEmpty())
            theme = Dtk::Gui::DGuiApplicationHelper::instance()->themeType()
                == Dtk::Gui::DGuiApplicationHelper::LightType ? "light" : "dark";
        const QString state = m_button->underMouse()
            ? (m_button->isDown() ? "press" : "hover") : "normal";
        const QString prefix = name == "tab_add" ? QString() : QStringLiteral("window_");
        const QIcon icon(QStringLiteral(":/other/gxde-window/%1%2_%3_%4.svg")
            .arg(prefix, name, theme, state));
        QPainter painter(m_button);
        if (!m_button->isEnabled())
            painter.setOpacity(0.4);
        // GXDE draws the original 40px asset centered in its 39px header.
        painter.drawPixmap(QPoint(0, (m_button->height() - 40) / 2),
            icon.pixmap(QSize(m_button->width(), 40), m_button->devicePixelRatioF()));

        if (m_button->hasFocus()) {
            painter.setPen(QPen(QColor("#2ca7f8"), 1, Qt::DotLine));
            painter.drawRect(m_button->rect().adjusted(2, 2, -3, -3));
        }
        return true;
    }

private:
    QAbstractButton *m_button;
    QString m_iconName;
};
}

void applyGxdeWindowButtonStyle(QAbstractButton *button, const QString &iconName)
{
    new GxdeWindowButtonStyle(button, iconName);
}

static const int VER_RESIZED_ALLOWED_OFF = 3;//允许的垂直偏移量
static const int VER_RESIZED_MIN_HEIGHT = 30;//resize的最小高度

DWIDGET_USE_NAMESPACE

TitleBar::TitleBar(QWidget *parent, bool showIcon) : QWidget(parent), m_layout(new QHBoxLayout(this))
{
    qCDebug(views) << "Enter TitleBar::TitleBar";
    Utils::set_Object_Name(this);
    m_layout->setObjectName("TitleBarLayout");//Add by ut001000 renfeixiang 2020-08-13
    /******** Modify by m000714 daizhengwen 2020-04-15: 标签栏和Dtk标签色保持一致****************/
//    DPalette palette = this->palette();
//    palette.setBrush(DPalette::Background, palette.color(DPalette::Base));
//    this->setPalette(palette);
    this->setBackgroundRole(DPalette::Base);
    this->setAutoFillBackground(false);
    /********************* Modify by m000714 daizhengwen End ************************/
    m_layout->setContentsMargins(0, 0, 10, 0);
    m_layout->setSpacing(0);
    // Normal windows use DTitlebar's icon; quake windows have no DTitlebar.
    if (showIcon) {
        auto logo = new QLabel(this);
        logo->setObjectName("GXDETitleIcon");
        logo->setFixedSize(48, WIN_TITLE_BAR_HEIGHT);
        logo->setAlignment(Qt::AlignCenter);
        logo->setPixmap(QIcon(":/logo/gxde-title.svg").pixmap(QSize(24, 24), devicePixelRatioF()));
        logo->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_layout->addWidget(logo);
    }

#ifdef DTKWIDGET_CLASS_DSizeMode
    setFixedHeight(DSizeModeHelper::element(WIN_TITLE_BAR_HEIGHT_COMPACT, WIN_TITLE_BAR_HEIGHT));
    QObject::connect(DGuiApplicationHelper::instance(), &DGuiApplicationHelper::sizeModeChanged, this, [this](){
        setFixedHeight(DSizeModeHelper::element(WIN_TITLE_BAR_HEIGHT_COMPACT, WIN_TITLE_BAR_HEIGHT));
    });
#else
    // daizhengwen fix bug#22927 动画出的矩形框会 -50 设置标题栏为50
    this->setFixedHeight(WIN_TITLE_BAR_HEIGHT);
#endif
}

TitleBar::~TitleBar()
{
    qCDebug(views) << "Enter TitleBar::~TitleBar";
}

void TitleBar::setTabBar(QWidget *widget)
{
    // qCDebug(views) << "Enter TitleBar::setTabBar";
    /******** Modify by n014361 wangpeili 2020-02-12: 修改居中样式***********×****/
    m_layout->addWidget(widget, 0, Qt::AlignVCenter);
    /***************** Modify by n014361 End ********************×****/
}

int TitleBar::rightSpace()
{
    // qCDebug(views) << "Enter TitleBar::rightSpace";
    return m_rightSpace;
}

void TitleBar::setVerResized(bool resized)
{
    // qCDebug(views) << "Enter TitleBar::setVerResized with resized:" << resized;
    setMouseTracking(true);
    m_verResizedEnabled = resized;
}


void TitleBar::mousePressEvent(QMouseEvent *event)
{
    // qCDebug(views) << "Enter TitleBar::mousePressEvent";
    QWidget *w = this->window();
    if(w) {
        // qCDebug(views) << "Branch: Parent window found, calculating mouse position";
        int windowMouseY = this->mapTo(w, event->pos()).y();
        int windowMouseYOff = w->height() - windowMouseY;
        m_verResizedCurOff = windowMouseYOff;
    }
    QWidget::mousePressEvent(event);
}

void TitleBar::mouseMoveEvent(QMouseEvent *event)
{
    // qCDebug(views) << "Enter TitleBar::mouseMoveEvent";
    forever {
        // qCDebug(views) << "Branch: Getting parent window";
        QWidget *w = this->window();
        if(!w) {
            qCDebug(views) << "No parent window found";
            break;
        }
        if(!m_verResizedEnabled) {
            qCDebug(views) << "Vertical resize disabled";
            break;
        }
        if(!this->hasMouseTracking()) {
            qCDebug(views) << "Mouse tracking disabled";
            break;
        }
        int windowMouseY = this->mapTo(w, event->pos()).y();
        int windowMouseYOff = w->height() - windowMouseY;
        if(event->buttons() != Qt::LeftButton && windowMouseYOff < VER_RESIZED_ALLOWED_OFF) {
            // qCDebug(views) << "Branch: Setting size cursor for vertical resize";
            this->setCursor(Qt::SizeVerCursor);
            break;
        }
        
        if(event->buttons() == Qt::LeftButton && m_verResizedCurOff < VER_RESIZED_ALLOWED_OFF) {
            // qCDebug(views) << "Branch: Performing window resize";
            w->resize(w->width(), qMax(VER_RESIZED_MIN_HEIGHT, windowMouseY + m_verResizedCurOff));
            break;
        }
        // qCDebug(views) << "Branch: Setting arrow cursor";
        this->setCursor(Qt::ArrowCursor);
        break;
    }
    // qCDebug(views) << "Branch: Calling parent mouse move event";
    QWidget::mouseMoveEvent(event);
}
