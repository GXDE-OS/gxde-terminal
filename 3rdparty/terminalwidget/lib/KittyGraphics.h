// SPDX-License-Identifier: GPL-3.0-or-later
// Kitty graphics protocol: Kovid Goyal and contributors.
// Reference: https://github.com/kovidgoyal/kitty
// Revision: 24f7369bb6385e06c44c35702a1478b51611510d
// Upstream graphics.c/.h: Copyright (C) 2017 Kovid Goyal
// <kovid at kovidgoyal.net>; distributed under the GPL3 license.
// Separately written Qt implementation; see THIRD_PARTY_NOTICES.md.

#ifndef KITTYGRAPHICS_H
#define KITTYGRAPHICS_H

#include <QByteArray>
#include <QHash>
#include <QImage>
#include <QPainter>
#include <QVector>

namespace Konsole {
// Per-screen graphics state. Coordinates are relative to the live screen;
// negative rows belong to scrollback. No file or shared-memory access.
class KittyGraphics
{
public:
    struct Placement {
        quint32 imageId, placementId;
        qint32 z;
        QRectF cells, source;
    };
    struct Result {
        QByteArray response;
        QSize cursorAdvance;
    };
    Result command(const QByteArray &data, QPoint cursor, QSize screen);
    void reset();
    void clearPlacements() { _placements.clear(); }
    void cancelUpload();
    void clearVisible(int lines);
    void trimHistory(int lines);
    void scroll(int top, int bottom, int delta, int historyLines, bool fullScreen = true);
    void paint(QPainter &painter, QPoint origin, int historyOffset, int layer, QSizeF logicalCellSize = QSizeF()) const;
    bool hasPlacements() const { return !_placements.isEmpty(); }
    const QVector<Placement> &placements() const { return _placements; }
    int imageCount() const { return _images.size(); }
    QSize cellSize = QSize(8, 16);
private:
    using Keys = QHash<char, qint64>;
    struct StoredImage { QImage pixels; quint32 number; quint64 serial; };
    QHash<quint32, StoredImage> _images;
    QVector<Placement> _placements;
    Keys _pending;
    QByteArray _payload;
    quint32 _nextId = 1;
    quint64 _serial = 0;
    qint64 _bytes = 0;
    void removeImage(quint32 id);
    quint32 resolveNumber(quint32 number) const;
};
}
#endif
