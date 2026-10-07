// SPDX-License-Identifier: GPL-3.0-or-later
// Kitty graphics protocol: Kovid Goyal and contributors.
// Reference: https://github.com/kovidgoyal/kitty
// Revision: 24f7369bb6385e06c44c35702a1478b51611510d
// Upstream graphics.c/.h: Copyright (C) 2017 Kovid Goyal
// <kovid at kovidgoyal.net>; distributed under the GPL3 license.
// Separately written Qt implementation; see THIRD_PARTY_NOTICES.md.
// Independent Qt implementation of kitty/docs/graphics-protocol.rst.

#include "KittyGraphics.h"
#include <QBuffer>
#include <QImageReader>
#include <QSet>
#include <algorithm>
#include <limits>
#include <cmath>
#include <zlib.h>

namespace Konsole {
namespace {
constexpr qint64 ImageLimit = 64 * 1024 * 1024;
constexpr qint64 StorageLimit = 128 * 1024 * 1024;
constexpr int PlacementLimit = 4096;
bool validSize(int w, int h) {
    return w > 0 && h > 0 && w <= 10000 && h <= 10000 && qint64(w) * h * 4 <= ImageLimit;
}
}
void KittyGraphics::cancelUpload() { _pending.clear(); _payload.clear(); }
void KittyGraphics::reset() {
    cancelUpload(); _images.clear(); _placements.clear(); _bytes = 0;
}
void KittyGraphics::removeImage(quint32 id) {
    auto it = _images.find(id);
    if (it == _images.end()) return;
    _bytes -= it->pixels.sizeInBytes();
    _images.erase(it);
    _placements.erase(std::remove_if(_placements.begin(), _placements.end(),
        [id](const Placement &p) { return p.imageId == id; }), _placements.end());
}
quint32 KittyGraphics::resolveNumber(quint32 number) const {
    quint64 newest = 0; quint32 id = 0;
    for (auto it = _images.cbegin(); it != _images.cend(); ++it)
        if (it->number == number && it->serial > newest) { newest = it->serial; id = it.key(); }
    return id;
}
KittyGraphics::Result KittyGraphics::command(const QByteArray &data, QPoint cursor, QSize screen)
{
    Result result;
    Keys keys;
    QByteArray error;
    const int separator = data.indexOf(';');
    const QByteArray control = separator < 0 ? data : data.left(separator);
    if (control.size() > 1024) error = "EINVAL:control data too long";
    for (const QByteArray &field : control.split(',')) {
        if (field.isEmpty()) continue;
        if (field.size() < 3 || field[1] != '=' || keys.contains(field[0])) { error = "EINVAL:invalid key"; break; }
        const char key = field[0];
        const QByteArray value = field.mid(2);
        qint64 number = 0;
        if (QByteArray("atod").contains(key)) {
            if (value.size() != 1) { error = "EINVAL:invalid character value"; break; }
            number = value[0];
        } else {
            bool ok = false;
            number = value.toLongLong(&ok);
            if (!ok || number > std::numeric_limits<quint32>::max() ||
                number < (key == 'z' || key == 'H' || key == 'V' ? std::numeric_limits<qint32>::min() : 0) ||
                (key == 'z' && number > std::numeric_limits<qint32>::max())) {
                error = "EINVAL:invalid integer"; break;
            }
        }
        keys.insert(key, number);
    }
    char action = char(keys.value('a', 't'));
    if (action == 'd') cancelUpload();
    else if (!_pending.isEmpty()) {
        // Continuations may contain only m and q (and repeated a=t/T).
        for (auto it = keys.cbegin(); it != keys.cend(); ++it)
            if (it.key() != 'm' && it.key() != 'q' && it.key() != 'a') error = "EINVAL:invalid continuation";
        Keys merged = _pending;
        merged['m'] = keys.value('m');
        if (keys.contains('q')) merged['q'] = keys['q'];
        keys = merged;
        action = char(keys.value('a', 't'));
    }
    quint32 id = quint32(keys.value('i'));
    const quint32 number = quint32(keys.value('I'));
    const quint32 placement = quint32(keys.value('p'));
    const int quiet = int(keys.value('q'));
    if (id && number) error = "EINVAL:i and I are mutually exclusive";
    if (quiet > 2 || keys.value('m') > 1 || keys.value('C') > 1) error = "EINVAL:invalid flag";
    if (keys.value('U') || keys.value('P') || keys.value('Q')) error = "ENOTSUP:virtual and relative placements";
    if (!QByteArray("tTqpd").contains(action)) error = "ENOTSUP:action";
    const bool transmitting = action == 't' || action == 'T' || action == 'q';
    if (transmitting && error.isEmpty()) {
        if (keys.value('t', 'd') != 'd') error = "ENOTSUP:use direct transmission (t=d)";
        if (keys.contains('o') && keys['o'] != 'z') error = "ENOTSUP:compression";
        const QByteArray encoded = separator < 0 ? QByteArray() : data.mid(separator + 1);
        // Strict base64 validation also works with Qt 5 versions predating the decoding status API.
        bool padding = false;
        int paddingCount = 0;
        for (char c : encoded) {
            if (c == '=') { padding = true; ++paddingCount; }
            else if (padding || !((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                     (c >= '0' && c <= '9') || c == '+' || c == '/')) error = "EINVAL:base64";
        }
        if (encoded.size() % 4 || paddingCount > 2) error = "EINVAL:base64";
        if (error.isEmpty()) {
            QByteArray decoded = QByteArray::fromBase64(encoded);
            if (_payload.size() + qint64(decoded.size()) > ImageLimit) error = "ENOSPC:upload limit";
            else _payload += decoded;
        }
        if (error.isEmpty() && keys.value('m') == 1) {
            _pending = keys;
            return result;
        }
    }
    QByteArray payload;
    if (transmitting) { payload.swap(_payload); _pending.clear(); }
    else if (number) id = resolveNumber(number);
    if (!error.isEmpty()) cancelUpload();
    QImage pixels;
    if (transmitting && error.isEmpty()) {
        if (keys.value('o') == 'z') {
            QByteArray inflated;
            z_stream stream = {};
            stream.next_in = reinterpret_cast<Bytef *>(payload.data());
            stream.avail_in = uInt(payload.size());
            int status = inflateInit(&stream);
            if (status == Z_OK) {
                char buffer[65536];
                do {
                    stream.next_out = reinterpret_cast<Bytef *>(buffer);
                    stream.avail_out = sizeof(buffer);
                    status = inflate(&stream, Z_NO_FLUSH);
                    inflated.append(buffer, int(sizeof(buffer) - stream.avail_out));
                } while (status == Z_OK && inflated.size() <= ImageLimit);
                const bool complete = status == Z_STREAM_END && stream.avail_in == 0;
                inflateEnd(&stream);
                if (!complete || inflated.size() > ImageLimit) error = "EINVAL:invalid or oversized zlib data";
            } else error = "EINVAL:zlib initialization";
            payload.swap(inflated);
        }
        const int format = int(keys.value('f', 32));
        if (error.isEmpty() && format == 100) {
            QBuffer buffer(&payload); buffer.open(QIODevice::ReadOnly);
            QImageReader reader(&buffer, "PNG");
            const QSize size = reader.size();
            if (!validSize(size.width(), size.height())) error = "EINVAL:PNG dimensions";
            else pixels = reader.read().convertToFormat(QImage::Format_RGBA8888);
        } else if (error.isEmpty() && (format == 24 || format == 32)) {
            const qint64 w = keys.value('s'), h = keys.value('v');
            if (w > 10000 || h > 10000 || !validSize(int(w), int(h)) || payload.size() != w * h * (format / 8))
                error = "EINVAL:pixel dimensions or length";
            else pixels = QImage(reinterpret_cast<const uchar *>(payload.constData()), int(w), int(h),
                        int(w) * (format / 8), format == 24 ? QImage::Format_RGB888 : QImage::Format_RGBA8888)
                        .convertToFormat(QImage::Format_RGBA8888).copy();
        } else if (error.isEmpty()) error = "ENOTSUP:pixel format";
        if (error.isEmpty() && pixels.isNull()) error = "EINVAL:image decode failed";
        if (error.isEmpty() && action != 'q') {
            if (!id) {
                do { id = _nextId++; } while (!id || _images.contains(id));
            }
            removeImage(id);
            while (_bytes + pixels.sizeInBytes() > StorageLimit || _images.size() >= 1024) {
                // Prefer evicting images without placements, then the oldest image.
                QSet<quint32> visible;
                for (const auto &p : _placements) visible.insert(p.imageId);
                auto oldest = _images.cbegin();
                for (auto it = _images.cbegin(); it != _images.cend(); ++it)
                    if ((visible.contains(oldest.key()) && !visible.contains(it.key())) ||
                        (visible.contains(oldest.key()) == visible.contains(it.key()) && it->serial < oldest->serial)) oldest = it;
                removeImage(oldest.key());
            }
            _images.insert(id, {pixels, number, ++_serial});
            _bytes += pixels.sizeInBytes();
        }
    }
    if (error.isEmpty() && (action == 'T' || action == 'p')) {
        if (!_images.contains(id)) error = "ENOENT:image not found";
        else {
            const QImage &image = _images[id].pixels;
            const qreal x = keys.value('x'), y = keys.value('y');
            const QRectF source = QRectF(x, y, keys.value('w') ? keys['w'] : image.width(), keys.value('h') ? keys['h'] : image.height())
                    .intersected(QRectF(image.rect()));
            const qreal ox = keys.value('X'), oy = keys.value('Y');
            qreal width = source.width(), height = source.height();
            const qreal cols = keys.value('c'), rows = keys.value('r');
            if (cols || rows) {
                const qreal scale = cols && rows ? qMin((cols * cellSize.width() - ox) / width, (rows * cellSize.height() - oy) / height)
                    : cols ? (cols * cellSize.width() - ox) / width : (rows * cellSize.height() - oy) / height;
                width *= scale; height *= scale;
            }
            if (source.isEmpty() || ox >= cellSize.width() || oy >= cellSize.height() || width <= 0 || height <= 0 ||
                cols > 10000 || rows > 10000) error = "EINVAL:placement geometry";
            else {
                if (placement) _placements.erase(std::remove_if(_placements.begin(), _placements.end(),
                    [id, placement](const Placement &p) { return p.imageId == id && p.placementId == placement; }), _placements.end());
                if (_placements.size() >= PlacementLimit) error = "ENOSPC:placement limit";
                else {
                    _placements.append({id, placement, qint32(keys.value('z')),
                        QRectF(cursor.x() + ox / cellSize.width(), cursor.y() + oy / cellSize.height(),
                               width / cellSize.width(), height / cellSize.height()), source});
                    std::stable_sort(_placements.begin(), _placements.end(), [](const Placement &a, const Placement &b) {
                        return a.z == b.z ? a.imageId < b.imageId : a.z < b.z;
                    });
                    
                    if (!keys.value('C')) result.cursorAdvance = QSize(int(std::ceil(cols ? cols : (ox + width) / cellSize.width())),
                                                                     int(std::ceil(rows ? rows : (oy + height) / cellSize.height())));
                }
            }
        }
    }
    if (error.isEmpty() && action == 'd') {
        const char mode = char(keys.value('d', 'a'));
        const char lower = char(mode >= 'A' && mode <= 'Z' ? mode + 'a' - 'A' : mode);
        if (!QByteArray("aincpqrxyz").contains(lower)) error = "ENOTSUP:deletion mode";
        else {
            QSet<quint32> removed;
            if ((lower == 'i' || lower == 'n') && id && !placement) removed.insert(id);
            if (lower == 'r') for (auto it = _images.cbegin(); it != _images.cend(); ++it)
                if (it.key() >= keys.value('x') && it.key() <= keys.value('y')) removed.insert(it.key());
            _placements.erase(std::remove_if(_placements.begin(), _placements.end(), [&](const Placement &p) {
                const QPointF point = lower == 'c' ? QPointF(cursor) : QPointF(keys.value('x') - 1, keys.value('y') - 1);
                const QRectF cell(point, QSizeF(1, 1));
                bool match = false;
                switch (lower) {
                case 'a': match = p.cells.intersects(QRectF(0, 0, screen.width(), screen.height())); break;
                case 'i': case 'n': match = p.imageId == id && (!placement || placement == p.placementId); break;
                case 'c': case 'p': case 'q': match = p.cells.intersects(cell) && (lower != 'q' || p.z == keys.value('z')); break;
                case 'r': match = removed.contains(p.imageId); break;
                case 'x': match = p.cells.left() < point.x() + 1 && p.cells.right() > point.x(); break;
                case 'y': match = p.cells.top() < point.y() + 1 && p.cells.bottom() > point.y(); break;
                case 'z': match = p.z == keys.value('z'); break;
                }
                if (match) removed.insert(p.imageId);
                return match;
            }), _placements.end());
            if (mode != lower) {
                for (const auto &p : _placements) removed.remove(p.imageId);
                for (quint32 removedId : removed) removeImage(removedId);
            }
        }
    }
    // Anonymous commands have no response. Honor response suppression for queries too.
    if ((keys.value('i') || number || action == 'q') &&
        (quiet != 2 && (quiet != 1 || !error.isEmpty()))) {
        result.response = "\033_Gi=" + QByteArray::number(id);
        if (number) result.response += ",I=" + QByteArray::number(number);
        if (placement) result.response += ",p=" + QByteArray::number(placement);
        result.response += ';' + (error.isEmpty() ? QByteArray("OK") : error) + "\033\\";
    }
    return result;
}
void KittyGraphics::trimHistory(int lines) {
    _placements.erase(std::remove_if(_placements.begin(), _placements.end(),
        [lines](const Placement &p) { return p.cells.bottom() <= -lines; }), _placements.end());
}
void KittyGraphics::translateRows(int delta) {
    for (auto &p : _placements) p.cells.translate(0, delta);
}
void KittyGraphics::clearVisible(int lines) {
    _placements.erase(std::remove_if(_placements.begin(), _placements.end(),
        [lines](const Placement &p) { return p.cells.bottom() > 0 && p.cells.top() < lines; }), _placements.end());
}
void KittyGraphics::scroll(int top, int bottom, int delta, int historyLines, bool fullScreen) {
    for (auto &p : _placements) {
        const bool fullScroll = fullScreen && top == 0 && delta < 0;
        if (fullScroll || (p.cells.top() >= top && p.cells.bottom() <= bottom + 1)) {
            p.cells.translate(0, delta);
            // Cursor advancement scrolls a newly placed image into view one row
            // at a time. During full-screen upward scrolling, retain pixels below
            // the viewport; painting clips them until they become visible.
            // Partial scrolling margins and expired history still crop the source.
            const qreal lo = fullScroll ? -historyLines : top;
            const qreal hi = fullScroll ? qMax(qreal(bottom + 1), p.cells.bottom()) : bottom + 1;
            const QRectF clipped = p.cells.intersected(QRectF(p.cells.x(), lo, p.cells.width(), hi - lo));
            if (clipped.isEmpty()) { p.cells = QRectF(); continue; }
            const qreal factor = p.source.height() / p.cells.height();
            p.source.setTop(p.source.top() + (clipped.top() - p.cells.top()) * factor);
            p.source.setHeight(clipped.height() * factor);
            p.cells = clipped;
        }
    }
    _placements.erase(std::remove_if(_placements.begin(), _placements.end(),
        [](const Placement &p) { return p.cells.isEmpty(); }), _placements.end());
}
void KittyGraphics::paint(QPainter &painter, QPoint origin, int historyOffset, int layer, QSizeF logicalCellSize) const {
    if (logicalCellSize.isEmpty()) logicalCellSize = cellSize;
    painter.save();
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    for (const auto &p : _placements) {
        const int imageLayer = p.z < -1073741824 ? -2 : p.z < 0 ? -1 : 1;
        if (imageLayer != layer) continue;
        QRectF target(origin.x() + p.cells.x() * logicalCellSize.width(),
                      origin.y() + (p.cells.y() + historyOffset) * logicalCellSize.height(),
                      p.cells.width() * logicalCellSize.width(), p.cells.height() * logicalCellSize.height());
        const auto it = _images.constFind(p.imageId);
        if (it != _images.cend()) painter.drawImage(target, it->pixels, p.source);
    }
    painter.restore();
}
}
