// SPDX-License-Identifier: GPL-3.0-or-later
#include "gxdesettingsstyle.h"
#include "titlebar.h"
#include <DBackgroundGroup>
#include <DListView>
#include <DTitlebar>
#include <DSlider>
#include <QAbstractButton>
#include <QBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPainter>
#include <QScrollArea>
#include <QStyleFactory>
#include <QStyledItemDelegate>

DWIDGET_USE_NAMESPACE

namespace {
bool isNavigationHeading(const QModelIndex &index)
{
    // DTK versions use different role numbers for the settings group key.
    const auto roles = index.model()->itemData(index);
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        if (it.key() >= Qt::UserRole && it.value().userType() == QMetaType::QString)
            return !it.value().toString().contains('.');
    return false;
}

class NavigationDelegate : public QStyledItemDelegate
{
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QSize sizeHint(const QStyleOptionViewItem &, const QModelIndex &index) const override
    {
        return QSize(160, index.data().toString().isEmpty() ? 20 :
                     (isNavigationHeading(index) && index.row() > 0 ? 50 : 30));
    }
    void paint(QPainter *p, const QStyleOptionViewItem &option, const QModelIndex &index) const override
    {
        const QString text = index.data().toString();
        if (text.isEmpty()) return;
        const bool heading = isNavigationHeading(index);
        QRect rect = option.rect;
        if (option.widget) rect.setRight(qMin(rect.right(), option.widget->width() - 1));
        rect.setTop(rect.bottom() - 29);
        const bool selected = option.state & QStyle::State_Selected;
        p->save();
        p->fillRect(option.rect, option.palette.color(QPalette::Window));
        if (selected) {
            p->fillRect(rect, QColor(43, 167, 248, 51));
            p->fillRect(QRect(rect.right() - 2, rect.top(), 3, rect.height()), QColor("#2ca7f8"));
        }
        QFont font = option.font;
        font.setPixelSize(heading ? 16 : 13);
        font.setBold(heading);
        p->setFont(font);
        p->setPen(selected ? QColor("#2ca7f8") : QColor(heading ? "#f0f0f0" : "#dedede"));
        p->drawText(rect.adjusted(heading ? 30 : 40, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
        p->restore();
    }
};

// DTK6 paints rounded row cards itself, independently of the widget style.
class FlatGroupBackground : public QObject
{
public:
    using QObject::QObject;
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::Paint) {
            auto widget = qobject_cast<QWidget *>(object);
            if (widget) {
                QPainter painter(widget);
                painter.fillRect(widget->rect(), widget->palette().color(QPalette::Window));
                return true;
            }
        }
        return false;
    }
};
}

void applyGxdeSettingsStyle(QWidget *dialog)
{
    dialog->setFixedSize(740, 670);
    QFont font = dialog->font();
    font.setPixelSize(13);
    dialog->setFont(font);
    auto style = QStyleFactory::create("ddark2");
    if (!style) style = QStyleFactory::create("Fusion");
    style->setParent(dialog);
    QPalette palette = style->standardPalette();
    palette.setColor(QPalette::Window, QColor("#252525"));
    palette.setColor(QPalette::Base, QColor("#303030"));
    palette.setColor(QPalette::Button, QColor("#353535"));
    palette.setColor(QPalette::ButtonText, QColor("#dedede"));
    palette.setColor(QPalette::WindowText, QColor("#dedede"));
    palette.setColor(QPalette::Text, QColor("#dedede"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#808080"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#808080"));
    palette.setColor(QPalette::Highlight, QColor("#2ca7f8"));
    auto widgets = dialog->findChildren<QWidget *>();
    widgets.prepend(dialog);
    for (auto widget : widgets) {
        if (qobject_cast<QMenu *>(widget)) continue;
        widget->setStyle(style);
        widget->setPalette(palette);
    }
    dialog->setAutoFillBackground(true);
    dialog->setStyleSheet(QStringLiteral(
        "QWidget#SettingDialog { background: #252525; color: #dedede; }"
        "QLabel { color: #dedede; background: transparent; }"
        "QScrollArea, QWidget#SettingsContent, QWidget#RightFrame { background: #252525; border: none; }"
        "QWidget#LeftFrame { background: #252525; border-right: 1px solid #404040; }"
        "QListView#NavigationBar { background: #252525; border: none; border-right: 1px solid #404040; padding: 0; }"
        "QComboBox, QSpinBox, QLineEdit { min-height: 22px; }"));
    for (auto area : dialog->findChildren<QScrollArea *>()) {
        if (area->accessibleName() != "ContentScrollArea") continue;
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        area->setWidgetResizable(true);
        if (auto content = area->widget()) {
            content->setMinimumWidth(0);
            content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        }
    }
    if (auto left = dialog->findChild<QWidget *>("LeftFrame")) left->setFixedWidth(160);
    if (auto nav = dialog->findChild<DListView *>("NavigationBar")) {
        nav->setFixedWidth(160);
        nav->setViewportMargins(0, 0, 0, 0);
        nav->setContentsMargins(0, 0, 0, 0);
        nav->setItemDelegate(new NavigationDelegate(nav));
        nav->setSpacing(0);
    }
    auto flat = new FlatGroupBackground(dialog);
    for (auto group : dialog->findChildren<DBackgroundGroup *>()) {
        group->installEventFilter(flat);
        group->setItemSpacing(4);
        group->setItemMargins(QMargins());
        if (group->layout()) group->layout()->setContentsMargins(13, 4, 10, 6);
        for (auto row : group->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly)) {
            auto layout = qobject_cast<QBoxLayout *>(row->layout());
            if (!layout) continue;
            layout->setContentsMargins(10, 5, 10, 5);
            layout->setSpacing(16);
            // Keep label/control columns aligned. Long translated labels grow
            // vertically instead of squeezing the editor into the remaining width.
            if (layout->count() < 2) continue;
            auto labelColumn = layout->itemAt(0)->widget();
            if (!labelColumn) continue;
            const auto labels = labelColumn->findChildren<QLabel *>();
            if (labels.isEmpty()) continue;
            labelColumn->setFixedWidth(190);
            for (auto label : labels) {
                label->setFixedWidth(190);
                label->setWordWrap(true);
                label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
                label->setMinimumHeight(label->heightForWidth(190));
            }
            layout->setStretch(0, 0);
            for (int i = 1; i < layout->count(); ++i)
                if (auto control = layout->itemAt(i)->widget())
                    layout->setAlignment(control, Qt::AlignVCenter);
        }
    }
    for (auto label : dialog->findChildren<QLabel *>("ContentTitleText")) {
        const bool heading = !label->parentWidget()->accessibleName().contains('.');
        QFont titleFont = font;
        titleFont.setPixelSize(heading ? 15 : 14);
        titleFont.setBold(true);
        label->setFont(titleFont);
        label->setForegroundRole(QPalette::WindowText);
        label->setStyleSheet("color: #dedede; background: transparent;");
    }
    for (auto slider : dialog->findChildren<DSlider *>()) {
        slider->setLeftIcon(QIcon());
        slider->setRightIcon(QIcon());
    }
    for (auto line : dialog->findChildren<QLabel *>("ContentTitleLine")) {
        const bool heading = !line->parentWidget()->accessibleName().contains('.');
        if (heading) {
            line->setFixedHeight(1);
            line->setStyleSheet("background: #404040;");
        } else line->hide();
    }
    for (auto title : dialog->findChildren<DTitlebar *>()) {
        title->setIcon(QIcon());
        title->setFixedHeight(39);
        title->setBackgroundTransparent(true);
        if (auto close = title->findChild<QAbstractButton *>("DTitlebarDWindowCloseButton")) {
            close->setProperty("gxdeWindowButtonTheme", "dark");
            applyGxdeWindowButtonStyle(close, "close");
        }
    }
}
