// SPDX-License-Identifier: GPL-3.0-or-later
#include "gxderemotestyle.h"
#include "titlebar.h"
#include <DGuiApplicationHelper>
#include <QApplication>
#include <QAbstractButton>
#include <QMenu>
#include <QLabel>
#include <QStyle>
#include <QStyleFactory>

namespace {
class RemoteStyle : public QObject
{
public:
    explicit RemoteStyle(QWidget *root) : QObject(root), m_root(root),
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
        // Covers newly created server/group dialogs and list rows as well as
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
        if (widget->objectName() == "RemoteIconLabel") {
            if (auto label = qobject_cast<QLabel *>(widget)) {
                label->setFixedSize(48, 39);
                label->setAlignment(Qt::AlignCenter);
                label->setPixmap(QIcon(":/logo/gxde-title.svg").pixmap(QSize(24, 24), label->devicePixelRatioF()));
            }
        }
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

void applyGxdeRemoteStyle(QWidget *root)
{
    new RemoteStyle(root);
}
