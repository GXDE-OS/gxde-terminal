// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef TERMINAL_UNICODE_H
#define TERMINAL_UNICODE_H

#include "konsole_wcwidth.h"
#include <QTextBoundaryFinder>
#include <QString>
#include <unicode/uchar.h>

namespace Konsole {
inline bool isEmojiCluster(const uint *points, int length)
{
    bool emoji = false, selector = false, presentation = false, keycap = false;
    for (int i = 0; i < length; ++i) {
        if (points[i] == 0xfe0e)
            return false; // Explicit text presentation.
        selector |= points[i] == 0xfe0f;
        keycap |= points[i] == 0x20e3;
        emoji |= u_hasBinaryProperty(points[i], UCHAR_EMOJI);
        presentation |= u_hasBinaryProperty(points[i], UCHAR_EMOJI_PRESENTATION);
    }
    return presentation || (emoji && (selector || keycap));
}

inline int terminalClusterWidth(const uint *points, int length)
{
    if (isEmojiCluster(points, length))
        return 2;
    int width = 0;
    for (int i = 0; i < length; ++i)
        width = qMax(width, characterWidth(points[i]));
    return width;
}

inline int terminalTextWidth(const QString &text)
{
    QTextBoundaryFinder boundaries(QTextBoundaryFinder::Grapheme, text);
    int width = 0, start = 0, end;
    while ((end = boundaries.toNextBoundary()) >= 0) {
        const auto points = text.mid(start, end - start).toUcs4();
        width += terminalClusterWidth(points.constData(), points.size());
        start = end;
    }
    return width;
}
}
#endif
