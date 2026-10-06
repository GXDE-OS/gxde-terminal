// SPDX-License-Identifier: GPL-3.0-or-later
// Four-corner exponential decay adapted from kitty/kitty/cursor_trail.c.
// kitty is Copyright (C) 2016-present Kovid Goyal and contributors.
#ifndef KONSOLE_CURSOR_ANIMATION_H
#define KONSOLE_CURSOR_ANIMATION_H

#include <QPolygonF>
#include <QRectF>
#include <cmath>

namespace Konsole {
class CursorAnimation
{
public:
    QPolygonF corners;
    QRectF target;
    bool active = false;
    QSizeF viewport = QSizeF(1, 1);

    void reset(const QRectF &rect = QRectF())
    {
        target = rect;
        corners = polygon(rect);
        active = false;
    }

    void moveTo(const QRectF &rect)
    {
        if (target.isEmpty()) {
            reset(rect);
            return;
        }
        if (target != rect) {
            target = rect;
            active = true;
        }
    }

    void advance(qreal seconds)
    {
        if (!active || seconds <= 0)
            return;
        const QPolygonF destination = polygon(target);
        const QPointF center = target.center();

        // kitty computes direction in normalized device coordinates, not pixels.
        // In a wide window this particularly affects diagonal carriage returns.
        const qreal sx = 1 / qMax(qreal(1), viewport.width());
        const qreal sy = 1 / qMax(qreal(1), viewport.height());
        const qreal radius = std::hypot(target.width() * sx, target.height() * sy) / 2;
        qreal dot[4];
        qreal minimum = 1, maximum = -1;
        for (int i = 0; i < 4; ++i) {
            const QPointF delta = destination[i] - corners[i];
            const QPointF direction(delta.x() * sx, delta.y() * sy);
            const QPointF corner((destination[i].x() - center.x()) * sx,
                                 (destination[i].y() - center.y()) * sy);
            const qreal length = std::hypot(direction.x(), direction.y());
            dot[i] = length > 0 ? QPointF::dotProduct(direction, corner) / (radius * length) : 0;
            minimum = qMin(minimum, dot[i]);
            maximum = qMax(maximum, dot[i]);
        }

        active = false;
        for (int i = 0; i < 4; ++i) {
            // kitty's default fast/slow decay times: 100/400 ms to 1/1024.
            const qreal decay = maximum == minimum ? 0.4
                : 0.4 - 0.3 * (dot[i] - minimum) / (maximum - minimum);
            corners[i] += (destination[i] - corners[i]) * (1 - std::exp2(-10 * seconds / decay));
            const QPointF remaining = destination[i] - corners[i];
            active |= qAbs(remaining.x()) >= 0.5 || qAbs(remaining.y()) >= 0.5;
        }
        if (!active)
            corners = destination;
    }

    QRect damage() const
    {
        return corners.boundingRect().united(target).toAlignedRect().adjusted(-2, -2, 2, 2);
    }

    static qreal blinkOpacity(qint64 elapsed, int period)
    {
        if (period <= 0)
            return 1;
        // Smooth ease-in-out over each half of the system blink period.
        return (1 + std::cos(6.283185307179586 * (elapsed % period) / period)) / 2;
    }

private:
    static QPolygonF polygon(const QRectF &rect)
    {
        QPolygonF result;
        result << rect.topRight() << rect.bottomRight() << rect.bottomLeft() << rect.topLeft();
        return result;
    }
};
}
#endif
