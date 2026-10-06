// SPDX-License-Identifier: GPL-3.0-or-later
#include "themepanel.h"
#include "gxderemotestyle.h"
#include "qtermwidget.h"
#include <DGuiApplicationHelper>
#include <QAbstractButton>
#include <QAction>
#include <QPainter>
#include <QScrollArea>
#include <QScrollBar>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QTimer>

namespace {
class ThemeCard : public QAbstractButton
{
public:
    ThemeCard(QAction *action, const QString &scheme, QWidget *parent)
        : QAbstractButton(parent), m_action(action), m_scheme(scheme)
    {
        setObjectName("ThemeCard");
        setText(action->text());
        setAccessibleName(action->text());
        setToolTip(action->text());
        setFixedHeight(54);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);
        connect(action, &QAction::changed, this, qOverload<>(&QWidget::update));
        connect(this, &QAbstractButton::clicked, action, &QAction::trigger);
    }
    bool selected() const { return m_action->isChecked(); }
protected:
    void paintEvent(QPaintEvent *) override
    {
        const QString scheme = m_scheme.isEmpty()
            ? (Dtk::Gui::DGuiApplicationHelper::instance()->themeType() == Dtk::Gui::DGuiApplicationHelper::LightType
                ? QStringLiteral("Light") : QStringLiteral("Dark")) : m_scheme;
        const auto colors = QTermWidget::colorSchemePreview(scheme);
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRectF box = QRectF(rect()).adjusted(1, 1, -1, -1);
        painter.setBrush(colors.value("background"));
        painter.setPen(QPen(selected() || hasFocus() ? QColor("#2ca7f8") : QColor(128, 128, 128, 70), selected() ? 2 : 1));
        painter.drawRoundedRect(box, 4, 4);
        if (underMouse()) painter.fillRect(rect().adjusted(3, 3, -3, -3), QColor(255, 255, 255, 12));
        painter.setClipRect(rect().adjusted(12, 4, -10, -4));
        QFont font = this->font();
        font.setPixelSize(14);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        int x = 14;
        const auto part = [&](const QString &text, QColor color) {
            painter.setPen(color);
            painter.drawText(x, 21, text);
            x += metrics.horizontalAdvance(text);
        };
        part("dde@linux", colors.value("host"));
        part(":", colors.value("foreground"));
        part("~/Theme", colors.value("path"));
        part("$ _", colors.value("foreground"));
        painter.setPen(colors.value("foreground"));
        painter.drawText(QRect(14, 27, width() - 28, 20), Qt::AlignVCenter,
                         metrics.elidedText(text(), Qt::ElideRight, width() - 28));
    }
private:
    QAction *m_action;
    QString m_scheme;
};
}

ThemePanel::ThemePanel(const QVector<Entry> &entries, QWidget *parent)
    : RightPanel(parent), m_scroll(new QScrollArea(this))
{
    setObjectName("ThemePanel");
    setFixedWidth(230);
    setGeometry(parent->width() - width(), 0, width(), parent->height());
    parent->installEventFilter(this);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto content = new QWidget;
    auto cards = new QVBoxLayout(content);
    cards->setContentsMargins(20, 5, 20, 5);
    cards->setSpacing(10);
    for (const auto &entry : entries) {
        auto card = new ThemeCard(entry.first, entry.second, content);
        card->installEventFilter(this);
        cards->addWidget(card);
        m_cards.append(card);
    }
    cards->addStretch();
    m_scroll->setWidget(content);
    auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_scroll);
    applyGxdePanelStyle(this);
    m_scroll->viewport()->setAutoFillBackground(false);
    content->setAutoFillBackground(false);
}

void ThemePanel::open()
{
    showAnim();
    QTimer::singleShot(0, this, [this] {
        for (auto widget : m_cards) {
            auto card = static_cast<ThemeCard *>(widget);
            card->update();
            if (card->selected()) {
                m_scroll->verticalScrollBar()->setValue(card->y() - (m_scroll->viewport()->height() - card->height()) / 2);
                card->setFocus(Qt::OtherFocusReason);
            }
        }
    });
}

bool ThemePanel::eventFilter(QObject *object, QEvent *event)
{
    if (object == parentWidget() && event->type() == QEvent::Resize)
        setGeometry(parentWidget()->width() - width(), 0, width(), parentWidget()->height());
    if (event->type() == QEvent::KeyPress && static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
        hideAnim();
        return true;
    }
    if (event->type() == QEvent::KeyPress) {
        const int key = static_cast<QKeyEvent *>(event)->key();
        const int index = m_cards.indexOf(qobject_cast<QWidget *>(object));
        if (index >= 0 && (key == Qt::Key_Up || key == Qt::Key_Down)) {
            m_cards[qBound(0, index + (key == Qt::Key_Down ? 1 : -1), int(m_cards.size()) - 1)]->setFocus(Qt::TabFocusReason);
            return true;
        }
        if (index >= 0 && (key == Qt::Key_Return || key == Qt::Key_Enter)) {
            static_cast<ThemeCard *>(m_cards[index])->click();
            return true;
        }
    }
    if (event->type() == QEvent::FocusIn) {
        if (auto widget = qobject_cast<QWidget *>(object)) m_scroll->ensureWidgetVisible(widget);
    }
    return RightPanel::eventFilter(object, event);
}
