// SPDX-License-Identifier: GPL-3.0-or-later
#ifndef KONSOLE_SCROLL_ANIMATION_H
#define KONSOLE_SCROLL_ANIMATION_H
#include <QtGlobal>

namespace Konsole {
// Like kitty/window.py ScrollAnimation: absolute pixel displacement, then an
// exact line boundary. Output scrolling uses 120 ms rather than key-repeat time.
class ScrollAnimation
{
public:
    qreal distance = 0;
    qreal elapsed = 0;
    bool active = false;
    void start(qreal pixels) { distance = pixels; elapsed = 0; active = pixels > 0; }
    void reset() { distance = elapsed = 0; active = false; }
    qreal offset() const { return active ? distance * (1 - qMin(qreal(1), elapsed / 0.12)) : 0; }
    void advance(qreal seconds)
    {
        elapsed += qMax(qreal(0), seconds);
        if (elapsed >= 0.12)
            reset();
    }
};
}
#endif
