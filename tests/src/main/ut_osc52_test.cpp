#include "Session.h"
#include "Emulation.h"
#include "Screen.h"
#include "ScreenWindow.h"
#include <gtest/gtest.h>
#include <QSignalSpy>

using namespace Konsole;
namespace {
void feed(Emulation *emulation, QByteArray data) {
    for (int i = 0; i < data.size(); ++i) emulation->receiveData(data.constData() + i, 1, false);
}
}
TEST(OSC52, StTerminatedSequenceDoesNotSwallowLaterOutput) {
    // OpenTUI (opencode) terminates OSC 52 with ST, not BEL.
    Session session;
    ScreenWindow *window = session.emulation()->createWindow();
    QSignalSpy spy(session.emulation(), SIGNAL(osc52ClipboardRequest(char,QString)));
    feed(session.emulation(), "\033]52;c;aGVsbG8=\033\\ok");
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy[0][0].value<char>(), 'c');
    EXPECT_EQ(spy[0][1].toString(), QString("aGVsbG8="));
    EXPECT_EQ(window->screen()->getCursorX(), 2);
}
TEST(OSC52, BelTerminatedTargetsAndLongPayloads) {
    Session session;
    ScreenWindow *window = session.emulation()->createWindow();
    QSignalSpy spy(session.emulation(), SIGNAL(osc52ClipboardRequest(char,QString)));
    const QByteArray payload(20000, 'A');
    session.emulation()->receiveData(QByteArray("\033]52;p;" + payload + "\007").constData(), 20008, false);
    feed(session.emulation(), "\033]52;;eA==\007ok");
    ASSERT_EQ(spy.count(), 2);
    EXPECT_EQ(spy[0][0].value<char>(), 'p');
    EXPECT_EQ(spy[0][1].toString(), QString(payload));
    EXPECT_EQ(spy[1][0].value<char>(), 'c');
    EXPECT_EQ(spy[1][1].toString(), QString("eA=="));
    EXPECT_EQ(window->screen()->getCursorX(), 2);
}
TEST(OSC52, CancelledSequenceResumesParsing) {
    Session session;
    ScreenWindow *window = session.emulation()->createWindow();
    QSignalSpy spy(session.emulation(), SIGNAL(osc52ClipboardRequest(char,QString)));
    // CAN aborts; an ESC that does not start ST aborts and begins a new escape.
    feed(session.emulation(), "\033]52;c;eA==\030a\033]52;c;eA==\033[Cb");
    EXPECT_EQ(spy.count(), 0);
    EXPECT_EQ(window->screen()->getCursorX(), 3);
}
