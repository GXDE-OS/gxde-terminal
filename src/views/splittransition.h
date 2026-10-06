// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QWidget>
#include <QPointer>
#include <QSplitter>
#include <QPainter>
#include <QVariantAnimation>
#include <QTimer>
#include <QStyle>
#include <QLayout>
#include <QEvent>

// Keep the actual terminal resize atomic: animate captured panes, not PTY sizes.
class SplitTransition : public QWidget
{
public:
    static SplitTransition *capture(QWidget *page, const QList<QWidget *> &panes)
    {
        if (auto previous = page->findChild<QWidget *>("SplitTransition", Qt::FindDirectChildrenOnly))
            delete previous;
        if (!page->isVisible() || !page->style()->styleHint(QStyle::SH_Widget_Animate, nullptr, page))
            return nullptr;
        return new SplitTransition(page, panes);
    }

    void play(const QList<QWidget *> &panes)
    {
        QList<QPointer<QWidget>> guarded;
        for (auto pane : panes) guarded.append(pane);
        QTimer::singleShot(0, this, [this, guarded] {
            hide();
            if (parentWidget()->layout()) parentWidget()->layout()->activate();
            m_background = parentWidget()->grab();
            for (auto &frame : m_frames) {
                frame.to = collapsed(frame.from, frame.direction);
                if (frame.pane && guarded.contains(frame.pane)) {
                    frame.to = geometryInPage(frame.pane);
                    frame.direction = direction(frame.pane);
                }
            }
            for (auto pane : guarded) {
                if (!pane) continue;
                bool existing = false;
                for (const auto &frame : m_frames) existing |= frame.pane == pane;
                if (existing) continue;
                Frame frame;
                frame.pane = pane;
                frame.to = geometryInPage(pane);
                frame.direction = direction(pane);
                frame.from = collapsed(frame.to, frame.direction);
                frame.image = pane->grab();
                frame.live = true;
                m_frames.append(frame);
            }
            show();
            raise();
            m_animation.start();
        });
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.drawPixmap(0, 0, m_background);
        for (const auto &frame : m_frames) {
            const QRectF rect(frame.from.topLeft() + (frame.to.topLeft() - frame.from.topLeft()) * m_progress,
                              frame.from.size() + (frame.to.size() - frame.from.size()) * m_progress);
            if (rect.isEmpty()) continue;
            // Reveal/crop at native scale so terminal glyphs never stretch.
            painter.save();
            painter.setClipRect(rect);
            // Opaque backing hides the final divider beneath translucent panes.
            QColor backing = frame.image.toImage().pixelColor(0, 0);
            backing.setAlpha(255);
            painter.fillRect(rect, backing);
            painter.drawPixmap(rect.topLeft(), frame.image);
            painter.restore();
            painter.setPen(QPen(palette().color(QPalette::Highlight), 1));
            if (frame.direction == Qt::Horizontal && rect.right() < width() - 1)
                painter.drawLine(rect.topRight(), rect.bottomRight());
            if (frame.direction == Qt::Vertical && rect.bottom() < height() - 1)
                painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        }
    }

    bool eventFilter(QObject *, QEvent *event) override
    {
        if (event->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            for (auto &frame : m_frames)
                if (frame.pane && parentWidget()->isAncestorOf(frame.pane))
                    frame.to = geometryInPage(frame.pane);
        }
        if (event->type() == QEvent::Hide) {
            m_animation.stop();
            hide();
            deleteLater();
        }
        return false;
    }

private:
    struct Frame {
        QPointer<QWidget> pane;
        QRectF from, to;
        QPixmap image;
        Qt::Orientation direction;
        bool live = false;
    };
    SplitTransition(QWidget *page, const QList<QWidget *> &panes)
        : QWidget(page), m_animation(this)
    {
        setObjectName("SplitTransition");
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        setGeometry(page->rect());
        m_background = page->grab();
        for (auto pane : panes)
            m_frames.append({pane, geometryInPage(pane), geometryInPage(pane), pane->grab(), direction(pane)});
        m_animation.setObjectName("SplitLayoutAnimation");
        m_animation.setDuration(240);
        m_animation.setStartValue(0.0);
        m_animation.setEndValue(1.0);
        m_animation.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
            m_progress = value.toReal();
            for (auto &frame : m_frames)
                if (frame.live && frame.pane) frame.image = frame.pane->grab();
            update();
        });
        connect(&m_animation, &QVariantAnimation::finished, this, [this] { hide(); deleteLater(); });
        page->installEventFilter(this);
        show();
        raise();
    }
    QRectF geometryInPage(QWidget *pane) const
    {
        return QRectF(pane->mapTo(parentWidget(), QPoint()), pane->size());
    }
    static Qt::Orientation direction(QWidget *pane)
    {
        auto splitter = qobject_cast<QSplitter *>(pane->parentWidget());
        return splitter ? splitter->orientation() : Qt::Horizontal;
    }
    static QRectF collapsed(QRectF rect, Qt::Orientation direction)
    {
        if (direction == Qt::Horizontal) rect.setLeft(rect.right());
        else rect.setTop(rect.bottom());
        return rect;
    }
    QList<Frame> m_frames;
    QPixmap m_background;
    QVariantAnimation m_animation;
    qreal m_progress = 0;
};
