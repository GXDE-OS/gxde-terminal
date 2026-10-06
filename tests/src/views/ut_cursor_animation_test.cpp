// SPDX-License-Identifier: GPL-3.0-or-later
#include "CursorAnimation.h"
#include "TerminalDisplay.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "Vt102Emulation.h"
#include <QApplication>
#include <QTimer>
#include <QPainter>
#include <QInputMethodEvent>
#include <QTest>
#include <gtest/gtest.h>

using namespace Konsole;

TEST(CursorAnimation, FirstPositionSnapsButWideGlyphChangesKeepMoving)
{
    CursorAnimation animation;
    animation.moveTo(QRectF(0, 0, 10, 20));
    EXPECT_FALSE(animation.active);
    animation.moveTo(QRectF(100, 50, 10, 20));
    EXPECT_TRUE(animation.active);
    animation.moveTo(QRectF(100, 50, 20, 20));
    EXPECT_TRUE(animation.active);
    animation.advance(1);
    EXPECT_FALSE(animation.active);
    EXPECT_EQ(animation.corners.boundingRect(), animation.target);
}

TEST(CursorAnimation, LongMoveAndReversalConvergeWithoutOvershoot)
{
    CursorAnimation animation;
    animation.reset(QRectF(0, 0, 10, 20));
    for (const QPointF &position : {QPointF(1200, 800), QPointF(0, 0)}) {
        animation.moveTo(QRectF(position, QSizeF(10, 20)));
        const QRectF bounds = animation.corners.boundingRect().united(animation.target);
        animation.advance(0.016);
        EXPECT_TRUE(animation.active);
        EXPECT_TRUE(bounds.contains(animation.corners.boundingRect()));
        for (int frame = 0; frame < 60 && animation.active; ++frame)
            animation.advance(1.0 / 60);
        EXPECT_FALSE(animation.active);
        EXPECT_EQ(animation.corners.boundingRect(), animation.target);
    }
    animation.moveTo(QRectF(500, 0, 10, 20));
    animation.advance(0.016);
    animation.moveTo(QRectF(0, 500, 10, 20));
    animation.advance(1.0); // Delayed frame after the event loop was busy.
    EXPECT_FALSE(animation.active);
    EXPECT_EQ(animation.corners.boundingRect(), animation.target);
}

TEST(CursorAnimation, BlinkIsSmoothPeriodicAndCanBeDisabled)
{
    EXPECT_DOUBLE_EQ(CursorAnimation::blinkOpacity(0, 1000), 1);
    EXPECT_NEAR(CursorAnimation::blinkOpacity(250, 1000), 0.5, 1e-9);
    EXPECT_NEAR(CursorAnimation::blinkOpacity(500, 1000), 0, 1e-9);
    EXPECT_DOUBLE_EQ(CursorAnimation::blinkOpacity(1000, 1000), 1);
    EXPECT_DOUBLE_EQ(CursorAnimation::blinkOpacity(500, 0), 1);
    for (int ms = 1; ms <= 1000; ++ms)
        EXPECT_LT(std::abs(CursorAnimation::blinkOpacity(ms, 1000)
                        - CursorAnimation::blinkOpacity(ms - 1, 1000)), 0.004);
}

TEST(CursorAnimation, DisplayStopsOnHiddenCursorFocusLossAndHide)
{
    Screen screen(100, 200);
    ScreenWindow window;
    window.setScreen(&screen);
    TerminalDisplay display;
    display.setVTFont(QFont(QStringLiteral("monospace"), 12));
    display.resize(600, 300);
    display.setScreenWindow(&window);
    display.show();
    display.setFocus();
    QApplication::processEvents();
    display.setBlinkingCursor(true);
    display.updateImage();
    ASSERT_TRUE(display.hasFocus());
    screen.setCursorYX(3, 20);
    window.notifyOutputChanged();
    ASSERT_TRUE(display._cursorTrail.active);
    ASSERT_TRUE(display._cursorTrailTimer->isActive());
    QTest::qWait(600);
    EXPECT_FALSE(display._cursorTrail.active);
    EXPECT_FALSE(display._cursorTrailTimer->isActive());
    EXPECT_TRUE(display._blinkCursorTimer->isActive());

    screen.resetMode(MODE_Cursor);
    window.notifyOutputChanged();
    EXPECT_FALSE(display._cursorTrailTimer->isActive());
    EXPECT_FALSE(display._blinkCursorTimer->isActive());
    screen.setMode(MODE_Cursor);
    window.notifyOutputChanged();
    EXPECT_FALSE(display._cursorTrail.active);
    EXPECT_TRUE(display._blinkCursorTimer->isActive());

    screen.setCursorYX(5, 5);
    window.notifyOutputChanged();
    display.clearFocus();
    EXPECT_FALSE(display._cursorTrailTimer->isActive());
    EXPECT_FALSE(display._blinkCursorTimer->isActive());
    EXPECT_DOUBLE_EQ(display._cursorOpacity, 1);
    display.setFocus();
    display.setBlinkingCursor(false);
    EXPECT_FALSE(display._blinkCursorTimer->isActive());
    EXPECT_DOUBLE_EQ(display._cursorOpacity, 1);
    screen.setCursorYX(2, 30);
    window.notifyOutputChanged();
    EXPECT_TRUE(display._cursorTrail.active);
    QInputMethodEvent preedit(QString::fromUtf8("中文"), {});
    QApplication::sendEvent(&display, &preedit);
    EXPECT_FALSE(display._cursorTrailTimer->isActive());
    EXPECT_FALSE(display._cursorTrail.active);
    QInputMethodEvent commit;
    QApplication::sendEvent(&display, &commit);
    screen.setCursorYX(4, 4);
    window.notifyOutputChanged();
    EXPECT_TRUE(display._cursorTrail.active);
    display.resize(620, 320);
    EXPECT_FALSE(display._cursorTrail.active);
    screen.setCursorYX(6, 20);
    window.notifyOutputChanged();
    EXPECT_FALSE(display._cursorTrail.active); // First frame establishes the new geometry.
    screen.setCursorYX(6, 30);
    window.notifyOutputChanged();
    EXPECT_TRUE(display._cursorTrail.active);
    display.hide();
    EXPECT_FALSE(display._cursorTrailTimer->isActive());
    EXPECT_FALSE(display._blinkCursorTimer->isActive());
}

TEST(CursorAnimation, CursorPaintingPreservesOpacityAndAllShapesFade)
{
    TerminalDisplay display;
    display.setVTFont(QFont(QStringLiteral("monospace"), 12));
    display.show();
    display.setFocus();
    QApplication::processEvents();
    ASSERT_TRUE(display.hasFocus());
    for (const auto shape : {Emulation::KeyboardCursorShape::BlockCursor,
                            Emulation::KeyboardCursorShape::IBeamCursor,
                            Emulation::KeyboardCursorShape::UnderlineCursor,
                            Emulation::KeyboardCursorShape::BoldUnderlineCursor}) {
        display.setKeyboardCursorShape(shape);
        int previous = 256;
        for (qreal opacity : {1.0, 0.5, 0.0}) {
            QImage image(40, 60, QImage::Format_ARGB32_Premultiplied);
            image.fill(Qt::black);
            QPainter painter(&image);
            display._cursorOpacity = opacity;
            bool invert = false;
            display.drawCursor(painter, QRect(0, 0, 12, display._fontHeight),
                               Qt::white, Qt::black, invert);
            EXPECT_DOUBLE_EQ(painter.opacity(), 1.0);
            painter.end();
            int brightest = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    brightest = qMax(brightest, qRed(image.pixel(x, y)));
            EXPECT_LT(brightest, previous);
            EXPECT_NEAR(brightest, opacity * 255, 1);
            previous = brightest;
        }
    }
}

// Exercise real VT input, including CR/LF and wrapping, rather than only CUP.
TEST(CursorAnimation, TerminalInputMovesWrapsAndReturnsContinuously)
{
    Vt102Emulation emulation;
    emulation.setImageSize(100, 60);
    ScreenWindow *window = emulation.createWindow();
    TerminalDisplay display;
    display.setVTFont(QFont(QStringLiteral("monospace"), 12));
    display.resize(720, 400);
    display.setScreenWindow(window);
    QObject::connect(&emulation, &Emulation::cursorChanged, &display,
                     [&display](Emulation::KeyboardCursorShape shape, bool blink) {
        display.setKeyboardCursorShape(shape);
        display.setBlinkingCursor(blink);
    });
    display.show();
    display.setFocus();
    QApplication::processEvents();
    display.updateImage();
    ASSERT_TRUE(display.hasFocus());
    auto feed = [&](const QByteArray &bytes) {
        emulation.receiveData(bytes.constData(), bytes.size(), false);
        emulation.showBulk();
    };
    auto settle = [&] {
        display._cursorTrail.advance(1);
        display._cursorTrailTimer->stop();
    };
    feed("prompt> ");
    ASSERT_TRUE(display._cursorTrail.active);
    settle();
    const QPolygonF beforeTyping = display._cursorTrail.corners;
    feed("x");
    EXPECT_TRUE(display._cursorTrail.active);
    EXPECT_EQ(display._cursorTrail.corners, beforeTyping);
    settle();
    const QRectF lineEnd = display._cursorTrail.target;
    feed("\r\n\x1b[2 q"); // A shell can reassert its cursor style at every prompt.
    ASSERT_TRUE(display._cursorTrail.active);
    EXPECT_EQ(display._cursorTrail.target.left(), 0);
    EXPECT_EQ(display._cursorTrail.target.top(), lineEnd.top() + display._fontHeight);
    EXPECT_EQ(display._cursorTrail.corners.boundingRect(), lineEnd);
    display._cursorTrail.advance(0.016);
    EXPECT_TRUE(display._cursorTrail.active);
    // The trail remains solid even when the stationary cursor is blinked out.
    display._cursorOpacity = 0;
    QImage frame(display.size(), QImage::Format_ARGB32_Premultiplied);
    frame.fill(Qt::transparent);
    QPainter painter(&frame);
    display.drawCursorTrail(painter);
    painter.end();
    int maximumAlpha = 0;
    for (int y = 0; y < frame.height(); ++y)
        for (int x = 0; x < frame.width(); ++x)
            maximumAlpha = qMax(maximumAlpha, qAlpha(frame.pixel(x, y)));
    EXPECT_GT(maximumAlpha, 240);
    settle();
    feed("\x1b[3;60H");
    settle();
    const QRectF beforeWrap = display._cursorTrail.target;
    feed("ab");
    ASSERT_TRUE(display._cursorTrail.active);
    EXPECT_EQ(display._cursorTrail.target.top(), beforeWrap.top() + display._fontHeight);
    EXPECT_EQ(display._cursorTrail.target.left(), display._fontWidth);
    EXPECT_EQ(display._cursorTrail.corners.boundingRect(), beforeWrap);
}

TEST(CursorAnimation, OutputScrollUsesAbsolutePixelsAndLandsExactly)
{
    ScrollAnimation animation;
    animation.start(24);
    animation.advance(0.03);
    EXPECT_NEAR(animation.offset(), 18, 1e-9);
    animation.advance(0.03);
    EXPECT_NEAR(animation.offset(), 12, 1e-9);
    animation.start(animation.offset() + 24);
    EXPECT_NEAR(animation.offset(), 36, 1e-9);
    animation.advance(0.12);
    EXPECT_FALSE(animation.active);
    EXPECT_DOUBLE_EQ(animation.offset(), 0);
}

TEST(CursorAnimation, BottomNewlineScrollsPixelsAndAlternateScreenDoesNot)
{
    Vt102Emulation emulation;
    emulation.setImageSize(100, 80);
    ScreenWindow *window = emulation.createWindow();
    TerminalDisplay display;
    display.setVTFont(QFont(QStringLiteral("monospace"), 12));
    display.resize(640, 300);
    display.setScreenWindow(window);
    QObject::connect(&emulation, &Emulation::primaryScreenInUse,
                     &display, &TerminalDisplay::setPrimaryScreen);
    display.show();
    display.setFocus();
    QApplication::processEvents();
    emulation.setImageSize(display._lines, display._columns);
    emulation.showBulk();
    ASSERT_TRUE(display.hasFocus());
    auto feed = [&](const QByteArray &bytes) {
        emulation.receiveData(bytes.constData(), bytes.size(), false);
        emulation.showBulk();
    };
    QByteArray content;
    for (int row = 1; row <= display._lines; ++row) {
        content += "\x1b[" + QByteArray::number(row) + ";1H\x1b["
            + QByteArray::number(41 + (row - 1) % 6) + "m\x1b[2Krow "
            + QByteArray::number(row);
    }
    feed(content);
    display._cursorTrail.advance(1);
    display._cursorTrailTimer->stop();
    display.setBlinkingCursor(false);
    const QImage before = display.grab().toImage();
    feed("\r\n");
    ASSERT_TRUE(display._outputScroll.active);
    ASSERT_TRUE(display._outputScrollTimer->isActive());
    EXPECT_EQ(display._outputScroll.distance, display._fontHeight);
    display._outputScrollTimer->stop(); // Deterministic rendering at half a frame.
    display._outputScroll.advance(0.06);
    const QImage middle = display.grab().toImage();
    const int x = display._fontWidth * 15;
    const int quarter = qMax(1, display._fontHeight / 4);
    auto pixel = [](const QImage &image, int x, int y) {
        return image.pixel(qRound(x * image.devicePixelRatio()), qRound(y * image.devicePixelRatio()));
    };
    EXPECT_EQ(pixel(middle, x, quarter), pixel(before, x, quarter));
    EXPECT_EQ(pixel(middle, x, 3 * quarter), pixel(before, x, display._fontHeight + quarter));
    display._outputScroll.advance(0.06);
    display.finishOutputScrollAnimation();
    const QImage after = display.grab().toImage();
    EXPECT_EQ(pixel(after, x, quarter), pixel(before, x, display._fontHeight + quarter));
    EXPECT_FALSE(display._outputScrollTimer->isActive());
    EXPECT_TRUE(display._outputScrollSnapshot.isNull());
    const QByteArray capture = qgetenv("FLAKE_ANIMATION_CAPTURE");
    if (!capture.isEmpty()) {
        before.save(QString::fromLocal8Bit(capture) + QStringLiteral("-before.png"));
        middle.save(QString::fromLocal8Bit(capture) + QStringLiteral("-middle.png"));
        after.save(QString::fromLocal8Bit(capture) + QStringLiteral("-after.png"));
    }
    feed("\r\n");
    ASSERT_TRUE(display._outputScroll.active);
    feed("\r\n"); // A second line arrives before the first one has settled.
    EXPECT_TRUE(display._outputScroll.active);
    EXPECT_GT(display._outputScroll.distance, display._fontHeight);
    feed("\x1b[?1049h");
    EXPECT_FALSE(display._outputScroll.active);
    EXPECT_FALSE(display._primaryScreen);
    feed("\x1b[" + QByteArray::number(display._lines) + ";1H\r\n");
    EXPECT_FALSE(display._outputScroll.active);
    feed("\x1b[?1049l");
    EXPECT_TRUE(display._primaryScreen);
    feed("\r\n");
    EXPECT_TRUE(display._outputScroll.active);
    QTest::qWait(180);
    EXPECT_FALSE(display._outputScroll.active);
    EXPECT_FALSE(display._outputScrollTimer->isActive());
    feed("\r\n");
    EXPECT_TRUE(display._outputScroll.active);
    display.hide();
    EXPECT_FALSE(display._outputScroll.active);
    EXPECT_FALSE(display._outputScrollTimer->isActive());
}
