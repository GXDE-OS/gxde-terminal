// SPDX-License-Identifier: GPL-3.0-or-later
#include "gxderemotestyle.h"
#include "titlebar.h"
#include <DGuiApplicationHelper>
#include <DSearchEdit>
#include <DIconButton>
#include <QApplication>
#include <QAbstractButton>
#include <QMenu>
#include <QLabel>
#include <QStyle>
#include <QStyleFactory>
#include <QLineEdit>
#include <QPainter>
#include <QVariantAnimation>
#include <QTimer>

namespace {
// DTK switches between two search icons. Draw one moving glyph while those
// native icons change visibility, then return painting to DTK at the endpoint.
class SearchFocusTransition : public QWidget
{
public:
    explicit SearchFocusTransition(Dtk::Widget::DSearchEdit *search)
        : QWidget(search), m_search(search), m_animation(this)
    {
        setObjectName("SearchFocusTransition");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        hide();
        m_animation.setObjectName("SearchFocusAnimation");
        m_animation.setDuration(180);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_rect = value.toRectF();
            update();
        });
        connect(&m_animation, &QVariantAnimation::finished, this, [this] { finish(); });
        for (auto icon : search->findChildren<Dtk::Widget::DIconButton *>())
            icon->installEventFilter(this);
        connect(search, &Dtk::Widget::DSearchEdit::textChanged, this, [this] { schedule(); });
        search->lineEdit()->installEventFilter(this);
        search->installEventFilter(this);
        schedule();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (object == m_search && (event->type() == QEvent::Hide || event->type() == QEvent::Resize
                || event->type() == QEvent::PaletteChange)) {
            m_animation.stop();
            finish();
            m_initialized = false;
            schedule();
        }
        if (qobject_cast<Dtk::Widget::DIconButton *>(object)
                && event->type() == QEvent::Paint && m_moving)
            return true;
        if (event->type() == QEvent::FocusIn || event->type() == QEvent::FocusOut
                || event->type() == QEvent::Show || event->type() == QEvent::Hide)
            schedule();
        return false;
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        m_icon.paint(&painter, m_rect.toAlignedRect(), Qt::AlignCenter,
                     m_search->isEnabled() ? QIcon::Normal : QIcon::Disabled);
    }

private:
    void finish()
    {
        m_moving = false;
        hide();
        for (auto icon : m_search->findChildren<Dtk::Widget::DIconButton *>()) icon->update();
    }

    void schedule()
    {
        if (m_pending) return;
        m_pending = true;
        // Let DTK finish updating its placeholder and editing layouts first.
        QTimer::singleShot(0, this, [this] {
            m_pending = false;
            if (!m_search->isVisible()) return;
            for (auto icon : m_search->findChildren<Dtk::Widget::DIconButton *>()) {
                if (!icon->isVisible() || icon->icon().isNull()) continue;
                const QPoint origin = icon->mapTo(m_search, QPoint());
                const QSize size = icon->iconSize();
                const QRectF target(QPointF(origin) + QPointF((icon->width() - size.width()) / 2.0,
                                                            (icon->height() - size.height()) / 2.0), size);
                m_icon = icon->icon();
                if (m_initialized && target == m_target) return;
                m_target = target;
                m_animation.stop();
                if (!m_initialized || !m_search->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, m_search)) {
                    m_initialized = true;
                    m_rect = target;
                    finish();
                    return;
                }
                setGeometry(m_search->rect());
                m_moving = true;
                show();
                raise();
                for (auto nativeIcon : m_search->findChildren<Dtk::Widget::DIconButton *>()) nativeIcon->update();
                m_animation.setStartValue(m_rect);
                m_animation.setEndValue(target);
                m_animation.start();
                return;
            }
        });
    }

    Dtk::Widget::DSearchEdit *m_search;
    QVariantAnimation m_animation;
    QIcon m_icon;
    QRectF m_rect;
    QRectF m_target;
    bool m_pending = false;
    bool m_initialized = false;
    bool m_moving = false;
};

class PanelStyle : public QObject
{
public:
    explicit PanelStyle(QWidget *root) : QObject(root), m_root(root),
        m_dark(QStyleFactory::create("ddark2")), m_light(QStyleFactory::create("dlight2"))
    {
        if (m_dark) m_dark->setParent(this);
        if (m_light) m_light->setParent(this);
        qApp->installEventFilter(this);
        connect(Dtk::Gui::DGuiApplicationHelper::instance(),
                &Dtk::Gui::DGuiApplicationHelper::themeTypeChanged, this, [this] {
            applyTree();
        });
        applyTree();
    }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        // Covers newly created panel dialogs and list rows as well as
        // combo-box popups. Menus retain the application's separate menu style.
        if (event->type() == QEvent::Polish || event->type() == QEvent::Show) {
            auto widget = qobject_cast<QWidget *>(object);
            if (widget && (widget == m_root || m_root->isAncestorOf(widget))) apply(widget);
        }
        return false;
    }

private:
    void applyTree()
    {
        apply(m_root);
        for (auto widget : m_root->findChildren<QWidget *>()) apply(widget);
    }
    void apply(QWidget *widget)
    {
        if (qobject_cast<QMenu *>(widget)) return;
        const bool dark = Dtk::Gui::DGuiApplicationHelper::instance()->themeType()
                == Dtk::Gui::DGuiApplicationHelper::DarkType;
        QStyle *style = dark ? m_dark : m_light;
        if (!style) return;
        if (widget->style() != style) widget->setStyle(style);
        QPalette palette = style->standardPalette();
        style->polish(palette);
        widget->setPalette(palette);
        if (auto search = qobject_cast<Dtk::Widget::DSearchEdit *>(widget)) {
            if (!search->findChild<QWidget *>("SearchFocusTransition", Qt::FindDirectChildrenOnly))
                new SearchFocusTransition(search);
        }
        if (auto icon = qobject_cast<Dtk::Widget::DIconButton *>(widget)) {
            for (auto parent = icon->parentWidget(); parent; parent = parent->parentWidget()) {
                if (auto search = qobject_cast<Dtk::Widget::DSearchEdit *>(parent)) {
                    if (search->objectName().startsWith("Remote")) {
                        // Both editing and placeholder glyphs use a small icon
                        // inside DTK's original slot to preserve vertical centering.
                        icon->setIconSize(QSize(12, 12));
                        icon->setFixedSize(20, 20);
                    }
                    break;
                }
                if (parent == m_root) break;
            }
        }
        if (widget == m_root || widget->inherits("CommonPanel"))
            widget->setAutoFillBackground(false);
        if ((widget->objectName() == "RemoteIconLabel" || widget->objectName() == "CustomLogoIcon")) {
            if (auto label = qobject_cast<QLabel *>(widget)) {
                label->setFixedSize(48, 39);
                label->setAlignment(Qt::AlignCenter);
                label->setPixmap(QIcon(":/logo/gxde-title.svg").pixmap(QSize(24, 24), label->devicePixelRatioF()));
            }
        }
        if (widget->objectName() == "CustomTitleBar")
            widget->setFixedHeight(39);
        if (widget->inherits("Dtk::Widget::DWindowCloseButton")) {
            widget->setProperty("gxdeWindowButtonTheme", dark ? "dark" : "light");
            if (!widget->property("gxdeCloseStyled").toBool()) {
                widget->setProperty("gxdeCloseStyled", true);
                applyGxdeWindowButtonStyle(qobject_cast<QAbstractButton *>(widget), "close");
            }
        }
    }
    QWidget *m_root;
    QStyle *m_dark;
    QStyle *m_light;
};
}

void applyGxdePanelStyle(QWidget *root)
{
    new PanelStyle(root);
}
