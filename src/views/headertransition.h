// (C) 2026 CharOfString <root@charofstring.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SRC_VIEWS_HEADERTRANSITION_H_
#define SRC_VIEWS_HEADERTRANSITION_H_

#include <QVariantAnimation>
#include <QWidget>
#include <QStyle>

class HeaderTransition : public QVariantAnimation {
public:
    explicit HeaderTransition(QWidget *widget, int duration = 140)
            : QVariantAnimation(widget), m_widget(widget) {
        setDuration(duration);
        setEasingCurve(QEasingCurve::OutCubic);
        setStartValue(0.0);
        setEndValue(0.0);
        connect(this, &QVariantAnimation::valueChanged, widget, [widget] {
            widget->update();
        });
    }

    qreal value() const {
        return currentValue().toReal();
    }

    void transitionTo(qreal target, bool animate = true) {
        if (!animate || !m_widget->isVisible()
                || !m_widget->style()->styleHint(QStyle::SH_Widget_Animate,
                    nullptr, m_widget)) {
            stop();
            setStartValue(target);
            setEndValue(target);
            return;
        }

        if (endValue().toReal() == target) {
            return;
        }

        const qreal from = value();
        stop();
        setStartValue(from);
        setEndValue(target);
        start();
    }

private:
    QWidget *m_widget;
};

#endif  // SRC_VIEWS_HEADERTRANSITION_H_
