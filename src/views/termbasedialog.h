// Copyright (C) 2019 ~ 2020 Uniontech Software Technology Co.,Ltd
// SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co., Ltd.
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef TERMBASEDIALOG_H
#define TERMBASEDIALOG_H

#include <DAbstractDialog>

// Qt port of master:widget/confirm_dialog.vala and master:style.css.
class TermCloseDialog : public Dtk::Widget::DAbstractDialog
{
    Q_OBJECT
public:
    TermCloseDialog(const QString &title, const QString &message,
                    const QString &cancelText, const QString &closeText,
                    QWidget *parent = nullptr);
protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private:
    void updateColors();
};

#endif // TERMBASEDIALOG_H
