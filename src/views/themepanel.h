// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "rightpanel.h"
#include <QPair>
#include <QVector>
class QAction;
class QScrollArea;
class ThemePanel : public RightPanel
{
public:
    using Entry = QPair<QAction *, QString>;
    explicit ThemePanel(const QVector<Entry> &entries, QWidget *parent);
    void open();
protected:
    bool eventFilter(QObject *object, QEvent *event) override;
private:
    QScrollArea *m_scroll;
    QVector<QWidget *> m_cards;
};
