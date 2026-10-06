// SPDX-License-Identifier: GPL-3.0-or-later
#include "gxdesettingsstyle.h"
#include <DWindowCloseButton>
#include "titlebar.h"
#include "headertransition.h"
#include <DBackgroundGroup>
#include <DBlurEffectWidget>
#include <DWindowManagerHelper>
#include <DGuiApplicationHelper>
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
class DialogCloseAlignment : public QObject {
public:
    DialogCloseAlignment(QWidget *dialog, DWindowCloseButton *button)
            : QObject(dialog), m_dialog(dialog), m_button(button) {
        if (button->parentWidget()->layout()) {
            button->parentWidget()->layout()->removeWidget(button);
        }

        button->setParent(dialog);
        dialog->installEventFilter(this);
        button->installEventFilter(this);
        sync();
    }

protected:
    bool eventFilter(QObject *, QEvent *event) override {
        if (!m_syncing && (event->type() == QEvent::Show ||
                event->type() == QEvent::Resize
                || event->type() == QEvent::Move ||
                event->type() == QEvent::LayoutRequest)) {
            sync();
        }
        return false;
    }

private:
    void sync() {
        m_syncing = true;
        m_button->setFixedSize(40, 39);
        m_button->move(m_dialog->width() - m_button->width(), 0);
        m_button->show();
        m_button->raise();
        m_syncing = false;
    }

    QWidget *m_dialog;
    DWindowCloseButton *m_button;
    bool m_syncing = false;
};

class SettingsBlur : public QObject
{
public:
    explicit SettingsBlur(QWidget *dialog) : QObject(dialog), m_dialog(dialog)
    {
        // Reuse the native dialog blur surface when DTK already provides one.
        m_blur = dialog->findChild<DBlurEffectWidget *>(QString(), Qt::FindDirectChildrenOnly);
        if (!m_blur) m_blur = new DBlurEffectWidget(dialog);
        m_blur->setObjectName("GXDESettingsBlur");
        m_blur->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_blur->setBlendMode(DBlurEffectWidget::BehindWindowBlend);
        m_blur->setFull(true);
        m_blur->setBlurEnabled(true);
        m_blur->setMaskColor(QColor("#252525"));
        m_blur->setMaskAlpha(210);
        m_blur->setBlurRectXRadius(8);
        m_blur->setBlurRectYRadius(8);
        dialog->setAttribute(Qt::WA_TranslucentBackground);
        dialog->installEventFilter(this);
        sync();
    }
protected:
    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Show)
            sync();
        return false;
    }
private:
    void sync()
    {
        // DTK may install a window palette while enabling its blur surface.
        auto palette = m_dialog->palette();
        palette.setColor(QPalette::Window, Qt::transparent);
        m_dialog->setPalette(palette);
        m_dialog->setAutoFillBackground(false);
        m_blur->setGeometry(m_dialog->rect());
        m_blur->lower();
        m_blur->show();
    }
    QWidget *m_dialog;
    DBlurEffectWidget *m_blur;
};

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
        if (index == m_hoverIndex && !selected) {
            p->fillRect(rect, QColor(option.palette.color(
                QPalette::WindowText).red(), option.palette.color(
                    QPalette::WindowText).green(), option.palette.color(
                        QPalette::WindowText).blue(),
                        qRound(14 * m_hover.value())));
        }
        const QRectF selection = m_selectionRect.isValid() ? m_selectionRect
            : (selected ? QRectF(rect) : QRectF());
        p->fillRect(selection, QColor(43, 167, 248, 51));
        p->fillRect(QRectF(selection.right() - 3, selection.top(), 3, selection.height()), QColor("#2ca7f8"));
        QFont font = option.font;
        font.setPixelSize(heading ? 16 : 13);
        font.setBold(heading);
        p->setFont(font);
        p->setPen(selected ? QColor("#2ca7f8") : option.palette.color(
            QPalette::WindowText));
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
                return true;
            }
        }
        return false;
    }
};
}

namespace {
void updateSettingsTheme(QWidget *dialog) {
    const bool dark = DGuiApplicationHelper::instance()
        ->themeType() == DGuiApplicationHelper::DarkType;

    const QString styleName = dark ? "ddark2" : "dlight2";
    auto style = dialog->findChild<QStyle *>(
        styleName, Qt::FindDirectChildrenOnly);

    if (!style) {
        style = QStyleFactory::create(styleName);
        if (!style) style = QStyleFactory::create("Fusion");
        style->setObjectName(styleName);
        style->setParent(dialog);
    }

    const QString text = dark ? "#dedede" : "#303030";
    const QString background = dark ? "#252525" : "#f5f5f5";
    const QString border = dark ? "#404040" : "#d4d4d4";

    QPalette palette = style->standardPalette();
    palette.setColor(QPalette::Window, Qt::transparent);
    palette.setColor(QPalette::Base, QColor(dark ? "#303030" : "#ffffff"));
    palette.setColor(QPalette::Button, QColor(dark ? "#353535" : "#eeeeee"));
    for (auto role : {QPalette::ButtonText, QPalette::WindowText,
            QPalette::Text}) {
        palette.setColor(role, QColor(text));
    }

    palette.setColor(QPalette::Highlight, QColor("#2ca7f8"));
    auto widgets = dialog->findChildren<QWidget *>();
    widgets.prepend(dialog);
    for (auto widget : widgets) {
        if (qobject_cast<QMenu *>(widget) || qobject_cast<DBlurEffectWidget *>(
                widget)) {
            continue;
    }

        widget->setStyle(style);
        widget->setPalette(palette);
    }

    auto blur = dialog->findChild<DBlurEffectWidget *>("GXDESettingsBlur");
    if (blur) {
        blur->setMaskColor(QColor(background));
        blur->setMaskAlpha(210);
    }

    dialog->setAutoFillBackground(false);
    dialog->setStyleSheet(QStringLiteral(
        "QWidget[gxdeSettingsSurface=\"true\"] { background: %1; color: %2; }"
        "QLabel { color: %2; background: transparent; }"
        "QScrollArea, QWidget#SettingsContent, QWidget#RightFrame { background: transparent; border: none; }"
        "QWidget#LeftFrame { background: transparent; border-right: 1px solid %3; }"
        "QListView#NavigationBar { background: transparent; border: none; border-right: 1px solid %3; padding: 0; }"
        "QComboBox, QSpinBox, QLineEdit { min-height: 22px; }")
        .arg(blur ? QStringLiteral("transparent") : background, text, border));

    for (auto label : dialog->findChildren<QLabel *>("ContentTitleText")) {
        label->setStyleSheet(QStringLiteral(
            "color: %1; background: transparent;").arg(text));
    }

    for (auto line : dialog->findChildren<QLabel *>("ContentTitleLine")) {
        line->setStyleSheet(QStringLiteral("background: %1;").arg(border));
    }

    for (auto close : dialog->findChildren<DWindowCloseButton *>()) {
        close->setProperty("gxdeWindowButtonTheme", dark ? "dark" : "light");
        close->update();
    }

    for (auto area : dialog->findChildren<QAbstractScrollArea *>()) {
        area->viewport()->setAutoFillBackground(false);
    }
    dialog->update();
}
}

void applyGxdeSettingsStyle(QWidget *dialog) {
    dialog->setFixedSize(740, 670);
    QFont font = dialog->font();
    font.setPixelSize(13);
    dialog->setFont(font);

    for (auto area : dialog->findChildren<QScrollArea *>()) {
        if (area->accessibleName() != "ContentScrollArea") continue;
        area->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        area->setWidgetResizable(true);
        area->viewport()->setAutoFillBackground(false);
        if (area->widget()) area->widget()->setAutoFillBackground(false);
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
        nav->viewport()->setAutoFillBackground(false);
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
        static_cast<QWidget *>(group)->setAutoFillBackground(false);
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
        label->setStyleSheet("background: transparent;");
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
    applyGxdeDialogStyle(dialog);
}

void applyGxdeDialogStyle(QWidget *dialog) {
    dialog->setProperty("gxdeSettingsSurface", true);
    QFont font = dialog->font();
    font.setPixelSize(13);
    dialog->setFont(font);
    if (DWindowManagerHelper::instance()->hasBlurWindow()) {
        new SettingsBlur(dialog);
    } else {
        // Keep an opaque readable surface on platforms without compositor blur.
        dialog->setStyleSheet(dialog->styleSheet() +
            QStringLiteral("QWidget[gxdeSettingsSurface=\"true\"] { background: #252525; }"));
    }

    // A null window icon inherits QApplication's terminal icon. Use an
    // explicit transparent icon for both native and DTK title bars.
    QPixmap emptyIcon(24, 24);
    emptyIcon.fill(Qt::transparent);
    const QIcon noTerminalIcon(emptyIcon);
    dialog->setWindowIcon(noTerminalIcon);
    for (auto title : dialog->findChildren<DTitlebar *>()) {
        title->setIcon(noTerminalIcon);
        title->setFixedHeight(39);
        title->setBackgroundTransparent(true);
        if (auto close = title->findChild<DWindowCloseButton *>()) {
            new DialogCloseAlignment(dialog, close);
        }
    }
    for (auto close : dialog->findChildren<DWindowCloseButton *>())
        applyGxdeWindowButtonStyle(close, "close");
    updateSettingsTheme(dialog);
    QObject::connect(DGuiApplicationHelper::instance(),
        &DGuiApplicationHelper::themeTypeChanged,
        dialog, [dialog] { updateSettingsTheme(dialog); });

}
