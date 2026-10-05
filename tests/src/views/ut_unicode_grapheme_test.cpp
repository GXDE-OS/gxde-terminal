// SPDX-License-Identifier: GPL-3.0-or-later
#include <gtest/gtest.h>
#include "Screen.h"
#include "TerminalCharacterDecoder.h"
#include <QTextStream>

using namespace Konsole;

static void feed(Screen &screen, const QString &text)
{
    for (uint point : text.toUcs4())
        screen.displayCharacter(point);
}

TEST(UnicodeGrapheme, CellWidthsAndCopyText)
{
    const QList<QPair<QString, int>> samples = {
        {QString::fromUtf8("😀"), 2}, {QString::fromUtf8("🥹"), 2},
        {QString::fromUtf8("🫠"), 2}, {QString::fromUtf8("❤️"), 2},
        {QString::fromUtf8("❤︎"), 1}, {QString::fromUtf8("👍🏽"), 2},
        {QString::fromUtf8("👨‍👩‍👧‍👦"), 2}, {QString::fromUtf8("👩‍💻"), 2},
        {QString::fromUtf8("🏳️‍🌈"), 2}, {QString::fromUtf8("🇨🇳"), 2},
        {QString::fromUtf8("1️⃣"), 2}, {QString::fromUtf8("1⃣"), 2}, {QString::fromUtf8("é"), 1},
        {QString::fromUtf8("中文"), 4}, {QString::fromUtf8("🇨🇳🇺🇸"), 4},
        {QString::fromUtf8("a😀b👍🏽c👩‍💻d🇨🇳e❤️f"), 16}
    };
    for (const auto &sample : samples) {
        SCOPED_TRACE(sample.first.toStdString());
        Screen screen(3, 80);
        feed(screen, sample.first + 'X');
        EXPECT_EQ(screen.getCursorX(), sample.second + 1);
        EXPECT_EQ(Character::stringWidth(sample.first), sample.second);
        QVector<Character> image(80);
        screen.getImage(image.data(), image.size(), 0, 0);
        QString copied;
        QTextStream stream(&copied);
        PlainTextDecoder decoder;
        decoder.begin(&stream);
        decoder.decodeLine(image.constData(), sample.second + 1, LINE_DEFAULT);
        decoder.end();
        EXPECT_EQ(copied, sample.first + 'X');
    }
}

TEST(UnicodeGrapheme, LateSelectorWrapsWholeCluster)
{
    Screen screen(3, 4);
    feed(screen, QString::fromUtf8("abc❤️X"));
    EXPECT_EQ(screen.getCursorY(), 1);
    EXPECT_EQ(screen.getCursorX(), 3);
    QVector<Character> image(8);
    screen.getImage(image.data(), image.size(), 0, 1);
    EXPECT_EQ(image[3].character, uint(' '));
    EXPECT_EQ(image[4].width(), 2);
    EXPECT_EQ(image[5].character, 0u);
    EXPECT_EQ(image[6].character, uint('X'));
}

TEST(UnicodeGrapheme, HtmlKeepsSequence)
{
    Screen screen(2, 20);
    const QString text = QString::fromUtf8("👩‍💻❤️");
    feed(screen, text);
    QVector<Character> image(20);
    screen.getImage(image.data(), image.size(), 0, 0);
    QString copied;
    QTextStream stream(&copied);
    HTMLDecoder decoder;
    decoder.setColorTable(base_color_table);
    decoder.begin(&stream);
    decoder.decodeLine(image.constData(), 4, LINE_DEFAULT);
    decoder.end();
    EXPECT_TRUE(copied.contains(text));
    EXPECT_FALSE(copied.contains(QChar(0)));
}
