// (C) 2026, CharOfString <root@charofstring.cc>
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef SRC_VIEWS_PALETTETRANSITION_H_
#define SRC_VIEWS_PALETTETRANSITION_H_

#include <QVariantAnimation>
#include <QWidget>
#include <QStyle>

inline void transitionPalette(QWidget *widget, const QPalette &target) {
    auto animation = widget->findChild<QVariantAnimation *>(
        "GxdePaletteTransition", Qt::FindDirectChildrenOnly);

    if (animation && animation->state() == QAbstractAnimation::Running
            && animation->property("targetPalette")
                .value<QPalette>() == target) {
        return;
    }

    if (animation) {
        animation->stop();
    }

    if (!widget->isVisible() || !widget->style()->styleHint(
            QStyle::SH_Widget_Animate, nullptr, widget)) {
        widget->setPalette(target);
        return;
    }

    if (!animation) {
        animation = new QVariantAnimation(widget);
        animation->setObjectName("GxdePaletteTransition");
        animation->setDuration(220);
        animation->setEasingCurve(QEasingCurve::OutCubic);
        QObject::connect(animation, &QVariantAnimation::valueChanged, widget,
                [widget, animation](const QVariant &value) {
            const auto from = animation->property("startPalette")
                .value<QPalette>();

            auto palette = animation->property("targetPalette")
                .value<QPalette>();
            const qreal t = value.toReal();

            for (auto group : {QPalette::Active, QPalette::Inactive,
                    QPalette::Disabled}) {
                for (auto role : {QPalette::Window, QPalette::Base,
                        QPalette::Button, QPalette::Highlight}) {
                    const QColor a = from.color(group, role), b = palette.color(
                        group, role);
                    palette.setColor(group, role, QColor::fromRgbF(
                        a.redF() + (b.redF() - a.redF()) * t,
                        a.greenF() + (b.greenF() - a.greenF()) * t,
                        a.blueF() + (b.blueF() - a.blueF()) * t,
                        a.alphaF() + (b.alphaF() - a.alphaF()) * t));
                }
            }
            widget->setPalette(palette);
            widget->update();
        });
    }
    animation->setProperty("startPalette", widget->palette());
    animation->setProperty("targetPalette", target);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->start();
}

# endif  // SRC_VIEWS_PALETTETRANSITION_H_
