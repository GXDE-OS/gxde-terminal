// SPDX-License-Identifier: GPL-3.0-or-later
#include "KittyGraphics.h"
#include "Session.h"
#include "TerminalDisplay.h"
#include "Emulation.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include "history/compact/CompactHistoryType.h"
#include <QScrollBar>
#include <QStyleFactory>
#include <gtest/gtest.h>
#include <QBuffer>
#include <QTest>
#include <zlib.h>
using namespace Konsole;
namespace {
QByteArray rawCommand(QByteArray keys = "a=T,i=1,C=1") {
    return keys + ",f=32,s=1,v=1;/wAA/w==";
}
KittyGraphics::Result send(KittyGraphics &g, QByteArray data, QPoint cursor = {}) {
    return g.command(data, cursor, QSize(80, 24));
}
void feed(Emulation *emulation, QByteArray data) {
    for (int i = 0; i < data.size(); ++i) emulation->receiveData(data.constData() + i, 1, false);
}
}
TEST(KittyGraphics, QueryDoesNotStoreAndChunkedUploadUsesFinalCursor) {
    KittyGraphics g;
    EXPECT_EQ(send(g, rawCommand("a=q,i=7")).response, QByteArray("\033_Gi=7;OK\033\\"));
    EXPECT_EQ(g.imageCount(), 0);
    EXPECT_TRUE(send(g, "a=T,i=7,f=32,s=1,v=1,m=1;/wAA").response.isEmpty());
    EXPECT_EQ(g.imageCount(), 0);
    auto r = send(g, "m=0;/w==", QPoint(3, 4));
    EXPECT_EQ(r.response, QByteArray("\033_Gi=7;OK\033\\"));
    ASSERT_EQ(g.placements().size(), 1);
    EXPECT_EQ(g.placements()[0].cells.topLeft(), QPointF(3, 4));
    EXPECT_EQ(r.cursorAdvance, QSize(1, 1));
}
TEST(KittyGraphics, FormatsCompressionAndBounds) {
    KittyGraphics g;
    QImage image(2, 3, QImage::Format_RGBA8888); image.fill(Qt::green);
    QByteArray png; QBuffer buffer(&png); buffer.open(QIODevice::WriteOnly); ASSERT_TRUE(image.save(&buffer, "PNG"));
    EXPECT_TRUE(send(g, "a=T,i=1,f=100,C=1;" + png.toBase64()).response.contains(";OK"));
    EXPECT_TRUE(send(g, "a=t,i=2,f=24,s=1,v=1;AP8A").response.contains(";OK"));
    QByteArray raw = QByteArray::fromHex("ff0000ff");
    uLongf size = compressBound(raw.size()); QByteArray compressed(int(size), '\0');
    ASSERT_EQ(compress(reinterpret_cast<Bytef *>(compressed.data()), &size, reinterpret_cast<const Bytef *>(raw.data()), raw.size()), Z_OK);
    compressed.resize(int(size));
    EXPECT_TRUE(send(g, "a=t,i=3,f=32,s=1,v=1,o=z;" + compressed.toBase64()).response.contains(";OK"));
    EXPECT_TRUE(send(g, "a=t,i=9,s=4294967295,v=4294967295;AAAA").response.contains("EINVAL"));
    EXPECT_TRUE(send(g, "a=t,i=9,s=1,v=1;!!!!").response.contains("EINVAL"));
    EXPECT_TRUE(send(g, "a=t,i=9,s=1,v=1,o=z;AAAA").response.contains("EINVAL"));
    EXPECT_EQ(g.imageCount(), 3);
}
TEST(KittyGraphics, PlacementDeletionNumbersAndSilencing) {
    KittyGraphics g;
    auto r = send(g, rawCommand("a=t,I=12"));
    EXPECT_TRUE(r.response.contains("I=12;OK"));
    EXPECT_TRUE(send(g, "a=p,I=12,p=5,C=1,c=4,r=4").response.contains(";OK"));
    ASSERT_EQ(g.placements().size(), 1);
    EXPECT_TRUE(send(g, "a=d,d=n,I=12,p=5").response.contains(";OK"));
    EXPECT_TRUE(g.placements().isEmpty()); EXPECT_EQ(g.imageCount(), 1);
    send(g, "a=p,I=12,C=1"); send(g, "a=d,d=N,I=12"); EXPECT_EQ(g.imageCount(), 0);
    EXPECT_TRUE(send(g, rawCommand("a=t,i=1,q=1")).response.isEmpty());
    EXPECT_TRUE(send(g, "a=p,i=99,q=1").response.contains("ENOENT"));
    EXPECT_TRUE(send(g, "a=p,i=99,q=2").response.isEmpty());
    EXPECT_TRUE(send(g, "a=p,i=1,U=1").response.contains("ENOTSUP"));
    EXPECT_TRUE(send(g, "a=q,i=99,t=f;L3RtcC94").response.contains("ENOTSUP"));
}
TEST(KittyGraphics, ScrollbackMarginsAndLayers) {
    KittyGraphics g; g.cellSize = QSize(10, 10);
    send(g, rawCommand("a=T,i=1,C=1,c=2,r=2,z=-1"), QPoint(2, 3));
    g.scroll(2, 10, -2, 0, false);
    ASSERT_EQ(g.placements().size(), 1);
    EXPECT_EQ(g.placements()[0].cells, QRectF(2, 2, 2, 1));
    EXPECT_EQ(g.placements()[0].source, QRectF(0, 0.5, 1, 0.5));
    QImage output(100, 100, QImage::Format_RGB32); output.fill(Qt::black);
    { QPainter p(&output); g.paint(p, QPoint(), 0, -1); }
    EXPECT_EQ(output.pixelColor(25, 25), QColor(Qt::red));
    g.scroll(0, 23, -4, 10);
    EXPECT_EQ(g.placements()[0].cells.top(), -2);
    g.clearVisible(24); EXPECT_EQ(g.placements().size(), 1);
    g.scroll(0, 23, -20, 10); EXPECT_TRUE(g.placements().isEmpty());
}
TEST(KittyGraphics, ParserFragmentationCancellationAndScreenIsolation) {
    Session session;
    ScreenWindow *window = session.emulation()->createWindow();
    QList<QByteArray> replies;
    QObject::connect(session.emulation(), &Emulation::sendData, session.emulation(),
        [&](const char *data, int length, const QTextCodec *) { replies.append(QByteArray(data, length)); });
    feed(session.emulation(), "\033_G" + rawCommand() + "\033\\");
    ASSERT_EQ(window->screen()->graphics.imageCount(), 1);
    ASSERT_EQ(replies.size(), 1);
    EXPECT_TRUE(replies[0].contains(";OK"));
    feed(session.emulation(), "\033_unknown APC\033\\");
    EXPECT_EQ(window->screen()->getCursorX(), 0);
    feed(session.emulation(), "\033_Ga=T,i=2,f=32,s=1,v=1;AAAA\030hello");
    EXPECT_EQ(window->screen()->getCursorX(), 5);
    feed(session.emulation(), "\033[?1049h");
    EXPECT_EQ(window->screen()->graphics.imageCount(), 0);
    feed(session.emulation(), "\033[?1049l");
    EXPECT_EQ(window->screen()->graphics.imageCount(), 1);
    feed(session.emulation(), "\033[2J");
    EXPECT_FALSE(window->screen()->graphics.hasPlacements());
    feed(session.emulation(), "\033c");
    EXPECT_EQ(window->screen()->graphics.imageCount(), 0);
    session.emulation()->setImageCellSize(QSize(9, 18));
    feed(session.emulation(), "\033[16t");
    EXPECT_EQ(replies.last(), QByteArray("\033[6;18;9t"));
}
TEST(KittyGraphics, FullScreenScrollPreservesPixelsBelowViewport) {
    KittyGraphics g; g.cellSize = QSize(10, 10);
    send(g, rawCommand("a=T,i=1,C=1,c=3,r=3"), QPoint(0, 23));
    for (int step = 1; step <= 2; ++step) {
        g.scroll(0, 23, -1, 0);
        ASSERT_EQ(g.placements().size(), 1);
        EXPECT_EQ(g.placements()[0].cells, QRectF(0, 23 - step, 3, 3));
        EXPECT_EQ(g.placements()[0].source, QRectF(0, 0, 1, 1));
    }
    // History expiry still clips the top and eventually removes the placement.
    g.scroll(0, 23, -22, 0);
    ASSERT_EQ(g.placements().size(), 1);
    EXPECT_EQ(g.placements()[0].cells, QRectF(0, 0, 3, 2));
    EXPECT_DOUBLE_EQ(g.placements()[0].source.height(), 2.0 / 3);
    g.scroll(0, 23, -2, 0);
    EXPECT_FALSE(g.hasPlacements());
}
TEST(KittyGraphics, RepeatedImagesAtBottomRenderEveryRow) {
    Session session;
    auto *emulation = session.emulation();
    emulation->setImageSize(6, 20);
    emulation->setImageCellSize(QSize(10, 10));
    ScreenWindow *window = emulation->createWindow();
    const QByteArray pixels = QByteArray::fromHex("ff0000ff00ff00ff0000ffff");
    for (int id = 1; id <= 2; ++id) {
        feed(emulation, "\033[6;1H\033_Ga=T,f=32,s=1,v=3,c=1,r=3,i="
             + QByteArray::number(id) + ';' + pixels.toBase64() + "\033\\");
        const auto &graphics = window->screen()->graphics;
        const auto &placement = graphics.placements().last();
        EXPECT_EQ(placement.cells, QRectF(0, 2, 1, 3));
        EXPECT_EQ(placement.source, QRectF(0, 0, 1, 3));
        EXPECT_EQ(window->screen()->getCursorY(), 5);
        QImage output(20, 6, QImage::Format_RGB32); output.fill(Qt::black);
        { QPainter painter(&output); graphics.paint(painter, QPoint(), 0, 1, QSizeF(1, 1)); }
        EXPECT_EQ(output.pixelColor(0, 2), QColor(Qt::red));
        EXPECT_EQ(output.pixelColor(0, 3), QColor(Qt::green));
        EXPECT_EQ(output.pixelColor(0, 4), QColor(Qt::blue));
    }
}
TEST(KittyGraphics, ImageOnlyUpdatesRepaintAndDelete) {
    Session session; TerminalDisplay view; session.addView(&view);
    view.setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
    view.resize(480, 240); view.show(); QTest::qWait(50);
    auto countRed = [&]() {
        QImage image = view.grab().toImage(); int count = 0;
        for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x)
            if (image.pixelColor(x, y) == QColor(Qt::red)) ++count;
        return count;
    };
    const int before = countRed();
    feed(session.emulation(), "\033_G" + rawCommand("a=T,i=1,C=1,c=6,r=3") + "\033\\");
    QTest::qWait(80); EXPECT_GT(countRed(), before + 100);
    feed(session.emulation(), "\033_Ga=d,d=A\033\\");
    QTest::qWait(80); EXPECT_EQ(countRed(), before);
}
TEST(KittyGraphics, FirstScrollbarAppearancePreservesImage) {
    Session session; TerminalDisplay view;
    view.setStyle(QStyleFactory::create("Fusion"));
    session.setHistoryType(CompactHistoryType(100));
    session.addView(&view);
    view.setVTFont(QFont(QStringLiteral("DejaVu Sans Mono"), 12));
    view.setScrollBarPosition(QTermWidget::NoScrollBar);
    view.resize(480, 240); view.show(); QTest::qWait(50);
    auto *screen = view.screenWindow()->screen();
    const int columns = screen->getColumns();
    const int lines = screen->getLines();
    // First fastfetch output fills the screen and exposes the scrollbar.
    feed(session.emulation(), "\033[2;1H\033_G" + rawCommand("a=T,i=1,C=1,c=3,r=3") + "\033\\");
    ASSERT_EQ(screen->graphics.placements().size(), 1);
    const QRectF expectedCells = screen->graphics.placements()[0].cells.translated(0, -1);
    feed(session.emulation(), QByteArray("\r\n").repeated(lines - 1));
    QTest::qWait(100);
    EXPECT_LT(screen->getColumns(), columns);
    ASSERT_TRUE(view.findChild<QScrollBar *>());
    EXPECT_TRUE(view.findChild<QScrollBar *>()->isVisible());
    ASSERT_EQ(screen->graphics.placements().size(), 1);
    EXPECT_EQ(screen->graphics.placements()[0].cells, expectedCells);
    QImage output = view.grab().toImage();
    int redPixels = 0;
    for (int y = 0; y < output.height(); ++y) for (int x = 0; x < output.width(); ++x)
        if (output.pixelColor(x, y) == QColor(Qt::red)) ++redPixels;
    EXPECT_GT(redPixels, 100);
}
TEST(KittyGraphics, ResizeMovesImageWithRowsEnteringAndLeavingHistory) {
    Session session;
    session.setHistoryType(CompactHistoryType(100));
    auto *emulation = session.emulation();
    emulation->setImageSize(6, 20);
    emulation->setImageCellSize(QSize(10, 10));
    auto *screen = emulation->createWindow()->screen();
    feed(emulation, "\033[4;1H\033_G" + rawCommand("a=T,i=1,C=1,c=2,r=2") + "\033\\\033[6;1H");
    emulation->setImageSize(4, 20);
    ASSERT_EQ(screen->graphics.placements().size(), 1);
    EXPECT_EQ(screen->graphics.placements()[0].cells, QRectF(0, 1, 2, 2));
    emulation->setImageSize(6, 20);
    ASSERT_EQ(screen->graphics.placements().size(), 1);
    EXPECT_EQ(screen->graphics.placements()[0].cells, QRectF(0, 3, 2, 2));
}
TEST(KittyGraphics, RejectsInvalidCommandsAndRecoversAfterAbortedChunks) {
    KittyGraphics g;
    EXPECT_TRUE(send(g, "a=t,i=1,I=2,s=1,v=1;/wAA/w==").response.contains("EINVAL"));
    EXPECT_TRUE(send(g, "a=t,i=1,z=2147483648,s=1,v=1;/wAA/w==").response.contains("EINVAL"));
    EXPECT_TRUE(send(g, "a=t,i=1,f=99,s=1,v=1;/wAA/w==").response.contains("ENOTSUP"));
    send(g, "a=t,i=1,s=1,v=1,m=1;/wAA");
    send(g, "a=d"); // A delete cancels pending upload metadata as well as bytes.
    EXPECT_TRUE(send(g, rawCommand("a=T,i=2,C=1")).response.contains(";OK"));
    ASSERT_EQ(g.imageCount(), 1);
    EXPECT_EQ(g.placements()[0].imageId, 2u);
    send(g, rawCommand("a=T,i=2,C=1"));
    EXPECT_EQ(g.placements().size(), 1); // Replacement removes old placements.
    send(g, "a=p,i=2,p=9,C=1,x=100");
    EXPECT_EQ(g.placements().size(), 1);
    EXPECT_TRUE(send(g, rawCommand("a=q,i=99,q=2")).response.isEmpty());
}
TEST(KittyGraphics, OversizedApcIsSwallowedAndParserResumes) {
    Session session;
    ScreenWindow *window = session.emulation()->createWindow();
    feed(session.emulation(), "\033_G" + QByteArray(20000, 'A') + "\033\\ok");
    EXPECT_EQ(window->screen()->getCursorX(), 2);
    EXPECT_EQ(window->screen()->graphics.imageCount(), 0);
    feed(session.emulation(), "\033_G" + rawCommand() + "\033\\");
    EXPECT_EQ(window->screen()->graphics.imageCount(), 1);
}
TEST(KittyGraphics, HiDpiPreservesEverySourcePixel) {
    KittyGraphics g;
    g.cellSize = QSize(20, 20); // Physical pixels, one cell at 2x device scale.
    QImage source(20, 20, QImage::Format_RGBA8888);
    for (int y = 0; y < 20; ++y) for (int x = 0; x < 20; ++x)
        source.setPixelColor(x, y, (x + y) % 2 ? Qt::white : Qt::black);
    QByteArray bytes; QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly);
    ASSERT_TRUE(source.save(&buffer, "PNG"));
    ASSERT_TRUE(send(g, "a=T,i=1,C=1,f=100;" + bytes.toBase64()).response.contains(";OK"));
    ASSERT_EQ(g.placements().size(), 1);
    EXPECT_EQ(g.placements()[0].cells.size(), QSizeF(1, 1));
    QImage output(20, 20, QImage::Format_RGBA8888);
    output.setDevicePixelRatio(2); output.fill(Qt::red);
    { QPainter painter(&output); g.paint(painter, QPoint(), 0, 1, QSizeF(10, 10)); }
    for (int y = 0; y < 20; ++y) for (int x = 0; x < 20; ++x)
        ASSERT_EQ(output.pixelColor(x, y), source.pixelColor(x, y)) << x << ',' << y;
}
TEST(KittyGraphics, FastfetchLayoutReturnsToImageTopRow) {
    Session session;
    auto *emulation = session.emulation();
    emulation->setImageSize(30, 80);
    emulation->setImageCellSize(QSize(9, 20));
    ScreenWindow *window = emulation->createWindow();
    // Captured with TERM_PROGRAM=gxde-terminal and --logo-width 25:
    // 225x223 pixels followed by CSI 12 A (ceil(223/20)). Start below a
    // command line: row zero would hide an extra cursor-up via clamping.
    const QByteArray commandLine("$ fastfetch --kitty logo.png --logo-width 25");
    feed(emulation, "\033[4;1H" + commandLine + "\r\n");
    QByteArray raw(225 * 223 * 4, char(0xff));
    uLongf length = compressBound(raw.size());
    QByteArray compressed(int(length), '\0');
    ASSERT_EQ(compress(reinterpret_cast<Bytef *>(compressed.data()), &length,
                       reinterpret_cast<const Bytef *>(raw.data()), raw.size()), Z_OK);
    compressed.resize(int(length));
    feed(emulation, "\033_Ga=T,f=32,s=225,v=223,o=z;" + compressed.toBase64() + "\033\\");
    EXPECT_EQ(window->screen()->getCursorY(), 16);
    feed(emulation, "\033[1G\033[12A\033[29C");
    EXPECT_EQ(window->screen()->getCursorY(), 4);
    EXPECT_EQ(window->screen()->getCursorX(), 29);
    ASSERT_EQ(window->screen()->graphics.placements().size(), 1);
    EXPECT_EQ(window->screen()->graphics.placements()[0].cells, QRectF(0, 4, 25, 11.15));
    feed(emulation, "char@host");
    QVector<Character> cells(30 * 80);
    window->screen()->getImage(cells.data(), cells.size(), 0, 29);
    for (int i = 0; i < commandLine.size(); ++i)
        EXPECT_EQ(cells[3 * 80 + i].character, uint(commandLine[i])) << i;
    EXPECT_EQ(cells[4 * 80 + 29].character, uint('c'));
}
