// SPDX-License-Identifier: GPL-3.0-or-later
#include "gxdesettingsstyle.h"
#include "titlebar.h"
#include "headertransition.h"
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
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QScrollBar>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QTimer>

DWIDGET_USE_NAMESPACE

namespace {
bool isNavigationHeading(const QModelIndex &index);

class SettingsScroll : public QObject
{
public:
    explicit SettingsScroll(QAbstractScrollArea *view, DListView *navigation = nullptr)
        : QObject(view), m_view(view), m_navigation(navigation), m_scroll(this), m_return(this), m_release(this)
    {
        m_origin = view->viewport()->pos();
        m_scroll.setObjectName(navigation ? "GXDESettingsNavigationScroll" : "GXDESettingsContentScroll");
        m_scroll.setDuration(130);
        m_scroll.setEasingCurve(QEasingCurve::OutCubic);
        m_return.setObjectName(navigation ? "GXDESettingsNavigationRebound" : "GXDESettingsContentRebound");
        m_return.setDuration(260);
        m_return.setEasingCurve(QEasingCurve::OutCubic);
        m_release.setSingleShot(true);
        m_release.setInterval(90);
        connect(&m_scroll, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_view->verticalScrollBar()->setValue(qRound(value.toReal()));
        });
        connect(&m_return, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            setOffset(value.toReal());
        });
        connect(&m_release, &QTimer::timeout, this, [this] { rebound(); });
        connect(view->verticalScrollBar(), &QScrollBar::sliderPressed, this, [this] { reset(); });
        connect(view->verticalScrollBar(), &QScrollBar::rangeChanged, this, [this] { reset(); });
        view->installEventFilter(this);
        view->viewport()->installEventFilter(this);
    }

    void stop() { reset(); }

protected:
    bool eventFilter(QObject *object, QEvent *event) override
    {
        if (event->type() == QEvent::Wheel && object == m_view->viewport()) {
            auto wheel = static_cast<QWheelEvent *>(event);
            if (wheel->modifiers() != Qt::NoModifier)
                return false;
            if (m_navigation) {
                navigate(wheel);
                wheel->accept();
                return true;
            }
            const bool pixelScroll = !wheel->pixelDelta().isNull();
            const qreal delta = pixelScroll ? wheel->pixelDelta().y()
                : wheel->angleDelta().y() / 120.0 * 3 * m_view->verticalScrollBar()->singleStep();
            if (qFuzzyIsNull(delta)) {
                if (wheel->phase() == Qt::ScrollEnd)
                    rebound();
                return false;
            }
            auto scrollbar = m_view->verticalScrollBar();
            const bool animate = m_view->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, m_view);
            const qreal previous = m_scroll.state() == QAbstractAnimation::Running
                ? m_scroll.endValue().toReal() : scrollbar->value();
            const qreal requested = previous - delta;
            const qreal target = qBound<qreal>(scrollbar->minimum(), requested, scrollbar->maximum());
            m_scroll.stop();
            if (pixelScroll || !animate) {
                scrollbar->setValue(qRound(target));
            } else if (!qFuzzyCompare(qreal(scrollbar->value()) + 1.0, target + 1.0)) {
                m_scroll.setStartValue(qreal(scrollbar->value()));
                m_scroll.setEndValue(target);
                m_scroll.start();
            }
            const qreal excess = requested - target;
            if (animate && !qFuzzyIsNull(excess)) {
                m_return.stop();
                // Moving the viewport keeps painted rows and hit testing in
                // the same coordinate system during the elastic displacement.
                setOffset(qBound(-24.0, m_offset - excess * 0.25, 24.0));
                m_release.start();
            } else {
                rebound();
            }
            if (wheel->phase() == Qt::ScrollEnd)
                rebound();
            wheel->accept();
            return true;
        }
        if (event->type() == QEvent::Hide || event->type() == QEvent::Resize) {
            reset();
            QTimer::singleShot(0, this, [this] {
                m_origin = m_view->viewport()->pos() - QPoint(0, qRound(m_offset));
            });
        } else if (event->type() == QEvent::MouseButtonPress) {
            // Keep the visible row still between press and release.
            m_scroll.stop();
            m_return.stop();
            m_release.stop();
        } else if (event->type() == QEvent::MouseButtonRelease) {
            rebound();
        } else if (event->type() == QEvent::KeyPress) {
            reset();
        }
        return false;
    }

private:
    void navigate(QWheelEvent *wheel)
    {
        const bool pixelScroll = !wheel->pixelDelta().isNull();
        const qreal delta = pixelScroll ? wheel->pixelDelta().y() : wheel->angleDelta().y();
        if (wheel->phase() == Qt::ScrollBegin || delta * m_navigationRemainder < 0)
            m_navigationRemainder = 0;
        m_navigationRemainder += delta;
        const qreal threshold = pixelScroll ? 40.0 : 120.0;
        while (qAbs(m_navigationRemainder) >= threshold) {
            const int direction = m_navigationRemainder < 0 ? 1 : -1;
            m_navigationRemainder += direction * threshold;
            auto model = m_navigation->model();
            int row = m_navigation->currentIndex().row() + direction;
            QModelIndex next;
            for (; row >= 0 && row < model->rowCount(); row += direction) {
                const auto candidate = model->index(row, 0);
                if (!m_navigation->isRowHidden(row) && !candidate.data().toString().isEmpty()
                    && !isNavigationHeading(candidate)
                    && (candidate.flags() & Qt::ItemIsEnabled)
                    && (candidate.flags() & Qt::ItemIsSelectable)) {
                    next = candidate;
                    break;
                }
            }
            if (next.isValid()) {
                rebound();
                m_navigation->setCurrentIndex(next);
                m_navigation->scrollTo(next, QAbstractItemView::EnsureVisible);
                // Use the same settings group activation as a mouse click.
                QMetaObject::invokeMethod(m_navigation, "clicked", Qt::DirectConnection,
                                          Q_ARG(QModelIndex, next));
            } else if (m_view->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, m_view)) {
                m_return.stop();
                setOffset(qBound(-24.0, m_offset - direction * 12.0, 24.0));
                m_release.start();
            }
        }
        if (wheel->phase() == Qt::ScrollEnd) {
            m_navigationRemainder = 0;
            rebound();
        }
    }

    void setOffset(qreal value)
    {
        m_offset = value;
        m_view->viewport()->move(m_origin + QPoint(0, qRound(value)));
    }
    void rebound()
    {
        m_release.stop();
        if (m_return.state() == QAbstractAnimation::Running || qFuzzyIsNull(m_offset))
            return;
        m_return.setStartValue(m_offset);
        m_return.setEndValue(0.0);
        m_return.start();
    }
    void reset()
    {
        m_scroll.stop();
        m_return.stop();
        m_release.stop();
        m_navigationRemainder = 0;
        setOffset(0.0);
    }

    QAbstractScrollArea *m_view;
    DListView *m_navigation;
    qreal m_navigationRemainder = 0.0;
    QPoint m_origin;
    QVariantAnimation m_scroll;
    QVariantAnimation m_return;
    QTimer m_release;
    qreal m_offset = 0.0;
};

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
    explicit NavigationDelegate(DListView *view)
        : QStyledItemDelegate(view), m_view(view), m_selection(this), m_hover(view->viewport())
    {
        // The viewport can be destroyed before this delegate. Keep the member
        // animation owned by the delegate, while repainting the viewport.
        m_hover.setParent(this);
        m_selection.setDuration(180);
        m_selection.setEasingCurve(QEasingCurve::OutCubic);
        m_selection.setObjectName("GXDESettingsSelectionAnimation");
        connect(&m_selection, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_selectionRect = value.toRectF();
            m_view->viewport()->update();
        });
        connect(view->selectionModel(), &QItemSelectionModel::currentChanged,
                this, [this] { updateSelection(true); });
        connect(view->verticalScrollBar(), &QScrollBar::valueChanged,
                this, [this] { updateSelection(false); });
        view->viewport()->setMouseTracking(true);
        view->viewport()->installEventFilter(this);
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        switch (event->type()) {
        case QEvent::MouseMove: {
            const auto index = m_view->indexAt(static_cast<QMouseEvent *>(event)->pos());
            if (index != m_hoverIndex) {
                m_hoverIndex = index;
                m_hover.transitionTo(0.0, false);
            }
            m_hover.transitionTo(index.isValid() ? 1.0 : 0.0);
            break;
        }
        case QEvent::Leave:
            m_hover.transitionTo(0.0);
            break;
        case QEvent::Resize:
        case QEvent::Show:
            updateSelection(false);
            break;
        case QEvent::Hide:
            m_selection.stop();
            m_hover.transitionTo(0.0, false);
            break;
        default:
            break;
        }
        return false;
    }
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
        p->setClipRect(option.rect, Qt::IntersectClip);
        p->fillRect(option.rect, option.palette.color(QPalette::Window));
        if (index == m_hoverIndex && !selected) {
            p->fillRect(rect, QColor(255, 255, 255, qRound(14 * m_hover.value())));
        }
        const QRectF selection = m_selectionRect.isValid() ? m_selectionRect
            : (selected ? QRectF(rect) : QRectF());
        p->fillRect(selection, QColor(43, 167, 248, 51));
        p->fillRect(QRectF(selection.right() - 3, selection.top(), 3, selection.height()), QColor("#2ca7f8"));
        QFont font = option.font;
        font.setPixelSize(heading ? 16 : 13);
        font.setBold(heading);
        p->setFont(font);
        p->setPen(selected ? QColor("#2ca7f8") : QColor(heading ? "#f0f0f0" : "#dedede"));
        p->drawText(rect.adjusted(heading ? 30 : 40, 0, -6, 0), Qt::AlignLeft | Qt::AlignVCenter, text);
        p->restore();
    }

private:
    void updateSelection(bool animate)
    {
        QRect target = m_view->visualRect(m_view->currentIndex());
        if (target.isValid()) {
            target.setRight(qMin(target.right(), m_view->viewport()->width() - 1));
            target.setTop(target.bottom() - 29);
        }
        if (m_selection.endValue().toRectF() == QRectF(target) && animate)
            return;
        m_selection.stop();
        if (!animate || !m_selectionRect.isValid() || !target.isValid() || !m_view->isVisible()
            || !m_view->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, m_view)) {
            m_selectionRect = target;
            m_selection.setStartValue(QRectF(target));
            m_selection.setEndValue(QRectF(target));
            m_view->viewport()->update();
            return;
        }
        m_selection.setStartValue(m_selectionRect);
        m_selection.setEndValue(QRectF(target));
        m_selection.start();
    }

    DListView *m_view;
    QVariantAnimation m_selection;
    HeaderTransition m_hover;
    QPersistentModelIndex m_hoverIndex;
    QRectF m_selectionRect;
};

class SettingsContentReveal : public QObject
{
public:
    SettingsContentReveal(QWidget *dialog, QWidget *content, DListView *navigation)
        : QObject(dialog), m_content(content), m_effect(new QGraphicsOpacityEffect(content)),
          m_animation(m_effect, "opacity", this)
    {
        content->setGraphicsEffect(m_effect);
        m_effect->setEnabled(false);
        m_animation.setObjectName("GXDESettingsContentAnimation");
        m_animation.setDuration(160);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QPropertyAnimation::finished, this, [this] { m_effect->setEnabled(false); });
        if (navigation) {
            connect(navigation, &DListView::clicked, this, [this] { reveal(0.75); });
            connect(navigation, &DListView::activated, this, [this] { reveal(0.75); });
        }
        dialog->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Show)
            reveal(0.0);
        else if (event->type() == QEvent::Hide) {
            m_animation.stop();
            m_effect->setEnabled(false);
        }
        return false;
    }

private:
    void reveal(qreal opacity)
    {
        if (!m_content->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, m_content))
            return;
        // Preserve the current opacity if another category is chosen mid-fade.
        const qreal from = m_animation.state() == QAbstractAnimation::Running ? m_effect->opacity() : opacity;
        m_animation.stop();
        m_effect->setEnabled(true);
        m_effect->setOpacity(from);
        m_animation.setStartValue(from);
        m_animation.setEndValue(1.0);
        m_animation.start();
    }

    QWidget *m_content;
    QGraphicsOpacityEffect *m_effect;
    QPropertyAnimation m_animation;
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
        auto scrolling = new SettingsScroll(area);
        if (auto nav = dialog->findChild<DListView *>("NavigationBar")) {
            QObject::connect(nav, &QAbstractItemView::clicked, scrolling, [scrolling] { scrolling->stop(); });
            QObject::connect(nav, &QAbstractItemView::activated, scrolling, [scrolling] { scrolling->stop(); });
        }
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
        nav->setMinimumHeight(0);
        nav->setMaximumHeight(QWIDGETSIZE_MAX);
        nav->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
        nav->setSizeAdjustPolicy(QAbstractScrollArea::AdjustIgnored);
        nav->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
        nav->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        nav->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        nav->setItemDelegate(new NavigationDelegate(nav));
        nav->setSpacing(0);
        new SettingsScroll(nav, nav);
    }
    if (auto content = dialog->findChild<QWidget *>("RightFrame")) {
        if (!content->graphicsEffect())
            new SettingsContentReveal(dialog, content, dialog->findChild<DListView *>("NavigationBar"));
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
        const QIcon icon(":/logo/gxde-title.svg");
        title->setIcon(QIcon(icon.pixmap(QSize(24, 24), title->devicePixelRatioF())));
        title->setFixedHeight(39);
        title->setBackgroundTransparent(true);
        if (auto close = title->findChild<QAbstractButton *>("DTitlebarDWindowCloseButton")) {
            close->setProperty("gxdeWindowButtonTheme", "dark");
            applyGxdeWindowButtonStyle(close, "close");
        }
    }
}
