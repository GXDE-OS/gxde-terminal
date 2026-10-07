// SPDX-License-Identifier: GPL-3.0-or-later
#include "Session.h"
#include "TerminalDisplay.h"
#include "Emulation.h"
#include <gtest/gtest.h>
#include <QTest>
#include <QPainter>
#include <algorithm>

using namespace Konsole;

TEST(TerminalColors, QueriesUseCurrentPaletteAndPreserveTerminators)
{
    Session session;
    TerminalDisplay view;
    session.addView(&view);
    ColorEntry palette[TABLE_COLORS];
    std::copy(view.colorTable(), view.colorTable() + TABLE_COLORS, palette);
    palette[DEFAULT_FORE_COLOR].color = QColor(0xab, 0xcd, 0xef);
    palette[DEFAULT_BACK_COLOR].color = QColor(0x12, 0x34, 0x56);
    view.setColorTable(palette);
    QList<QByteArray> replies;
    QObject::connect(session.emulation(), &Emulation::sendData, &view,
                     [&replies](const char *data, int length, const QTextCodec *) {
        replies.append(QByteArray(data, length));
    });
    const QByteArray queries("\033]10;?\007\033]11;?\033\\\033]11;?\007");
    // Feed bytes individually to exercise queries split across PTY reads.
    for (int i = 0; i < queries.size(); ++i)
        session.emulation()->receiveData(queries.constData() + i, 1, false);
    ASSERT_EQ(replies.size(), 3);
    EXPECT_EQ(replies[0], QByteArray("\033]10;rgb:abab/cdcd/efef\007"));
    EXPECT_EQ(replies[1], QByteArray("\033]11;rgb:1212/3434/5656\033\\"));
    EXPECT_EQ(replies[2], QByteArray("\033]11;rgb:1212/3434/5656\007"));
    palette[DEFAULT_BACK_COLOR].color = QColor(0xff, 0xff, 0xff);
    view.setColorTable(palette);
    const QByteArray query("\033]11;?\007");
    session.emulation()->receiveData(query.constData(), query.size(), false);
    ASSERT_EQ(replies.size(), 4);
    EXPECT_EQ(replies[3], QByteArray("\033]11;rgb:ffff/ffff/ffff\007"));
}

TEST(TerminalColors, RepaintsSuccessiveTrueColorBackgroundFrames)
{
    Session session;
    TerminalDisplay view;
    session.addView(&view);
    view.setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
    view.resize(480, 240);
    view.show();
    for (const QColor color : {QColor(32, 45, 60), QColor(70, 85, 100)}) {
        const QByteArray frame = QStringLiteral("\033[H\033[48;2;%1;%2;%3m          \033[0m")
            .arg(color.red()).arg(color.green()).arg(color.blue()).toLatin1();
        session.emulation()->receiveData(frame.constData(), frame.size(), false);
        QTest::qWait(60);
        const QImage image = view.grab().toImage();
        int pixels = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y).rgb() == color.rgb())
                    ++pixels;
        EXPECT_GT(pixels, 100) << "Background frame was not painted";
    }
}

TEST(TerminalColors, PartialRepaintPreservesBackgroundAtRowBottom)
{
    Session session;
    TerminalDisplay view;
    session.addView(&view);
    view.setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
    view.setMargin(3);
    view.resize(480, 240);
    view.show();
    const QByteArray frame("\033[2;1H\033[48;2;55;57;65m          \033[0m");
    session.emulation()->receiveData(frame.constData(), frame.size(), false);
    QTest::qWait(60);
    QImage image = view.grab().toImage();
    const QRect row = view.imageToWidget(QRect(2, 1, 6, 1));
    const QRect strip(row.left(), row.bottom() - 1, row.width(), 2);
    // Reproduce a narrow dirty region from cursor/animation updates. The
    // background clear must be followed by repainting the same terminal row.
    QPainter painter(&image);
    painter.setClipRect(strip);
    painter.fillRect(strip, Qt::black);
    view.drawContents(painter, view.widgetToImage(strip));
    painter.end();
    EXPECT_EQ(image.pixelColor(strip.center()), QColor(55, 57, 65));
}

TEST(TerminalColors, RemoteBackgroundSequenceDoesNotAbortAndQueriesSeeNewColor)
{
    Session session;
    TerminalDisplay first, second;
    for (TerminalDisplay *view : {&first, &second}) {
        view->setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
        view->resize(480, 240);
        session.addView(view);
        view->show();
    }
    const QColor foreground = first.colorTable()[DEFAULT_FORE_COLOR].color;
    QList<QByteArray> replies;
    QObject::connect(session.emulation(), &Emulation::sendData, &first,
                     [&replies](const char *data, int length, const QTextCodec *) {
        replies.append(QByteArray(data, length));
    });
    const QByteArray stream("\033]11;#123456\007\033]11;?\007"
                            "\033]11;rgb:ab/cd/ef\033\\\033]11;?\033\\");
    for (int i = 0; i < stream.size(); ++i)
        session.emulation()->receiveData(stream.constData() + i, 1, false);
    ASSERT_EQ(replies.size(), 2);
    EXPECT_EQ(replies[0], QByteArray("\033]11;rgb:1212/3434/5656\007"));
    EXPECT_EQ(replies[1], QByteArray("\033]11;rgb:abab/cdcd/efef\033\\"));
    EXPECT_EQ(first.colorTable()[DEFAULT_BACK_COLOR].color, QColor(0xab, 0xcd, 0xef));
    EXPECT_EQ(second.colorTable()[DEFAULT_BACK_COLOR].color, QColor(0xab, 0xcd, 0xef));
    EXPECT_EQ(first.palette().color(first.backgroundRole()), QColor(0xab, 0xcd, 0xef));
    EXPECT_EQ(first.colorTable()[DEFAULT_FORE_COLOR].color, foreground);
    QTest::qWait(40); // No queued title update may revert the background later.
    EXPECT_EQ(first.colorTable()[DEFAULT_BACK_COLOR].color, QColor(0xab, 0xcd, 0xef));
}

TEST(TerminalColors, BackgroundCommandsValidateRgbAndReapplyAfterThemeChange)
{
    Session session;
    TerminalDisplay view;
    session.addView(&view);
    auto feed = [&](const QByteArray &sequence) {
        session.emulation()->receiveData(sequence.constData(), sequence.size(), false);
    };
    feed("\033]11;rgb:f/80/0000\007");
    EXPECT_EQ(view.colorTable()[DEFAULT_BACK_COLOR].color, QColor(255, 128, 0));
    for (const QByteArray &color : {QByteArray("not-a-color"), QByteArray("rgb:1/2"),
                                   QByteArray("rgb:10000/0/0"), QByteArray("rgb:+1/0/0"),
                                   QByteArray("rgb:gg/00/00"), QByteArray("rgb:/0/0")}) {
        feed("\033]11;" + color + "\007");
        EXPECT_EQ(view.colorTable()[DEFAULT_BACK_COLOR].color, QColor(255, 128, 0));
    }
    // Receiving the same OSC value again must apply even after a theme change.
    view.setBackgroundColor(Qt::black);
    feed("\033]11;rgb:f/80/0000\007");
    EXPECT_EQ(view.colorTable()[DEFAULT_BACK_COLOR].color, QColor(255, 128, 0));
}

TEST(TerminalColors, RemoteBackgroundSurvivesPaletteAnimation)
{
    Session session;
    TerminalDisplay view;
    session.addView(&view);
    view.setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
    view.resize(480, 240);
    view.show();
    ColorEntry palette[TABLE_COLORS];
    std::copy(view.colorTable(), view.colorTable() + TABLE_COLORS, palette);
    palette[DEFAULT_BACK_COLOR].color = Qt::blue;
    view.setColorTable(palette);
    const QByteArray sequence("\033]11;#123456\007");
    session.emulation()->receiveData(sequence.constData(), sequence.size(), false);
    EXPECT_EQ(view.colorTable()[DEFAULT_BACK_COLOR].color, QColor(0x12, 0x34, 0x56));
    QTest::qWait(300);
    EXPECT_EQ(view.colorTable()[DEFAULT_BACK_COLOR].color, QColor(0x12, 0x34, 0x56));
}
