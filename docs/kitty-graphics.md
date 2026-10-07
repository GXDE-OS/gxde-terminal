# Kitty graphics protocol

GXDE Terminal implements the static-image portion of the Kitty graphics protocol
in its Qt terminal widget. Image data belongs to each screen buffer independently.

## Usage

For Kitty's image client, select direct transmission:

```sh
kitten icat --transfer-mode=stream photo.png
```

For fastfetch, explicitly select the Kitty backend and a logo width in cells:

```sh
fastfetch --kitty /path/to/logo.png --logo-width 30
```

With fastfetch 2.56.1, `--logo /path/to/logo.png` alone can select character-art
output in GXDE Terminal; it does not necessarily select the Kitty protocol.
Without an explicit width, a large source image can also occupy most of the
terminal. Backend detection is controlled by the client.

Other clients should use `t=d`. This works over SSH as well as locally.
A dependency-free smoke test, run inside GXDE Terminal:

```sh
printf '\033_Ga=T,f=24,s=1,v=1,c=8,r=4,C=1,q=2;/wAA\033\\'
# Remove visible images:
printf '\033_Ga=d,d=A\033\\'
```

## Implemented

- APC framing (`ESC _ G ... ESC \\`), including fragmented PTY reads, cancellation,
  and consuming unknown APC strings without printing their payload.
- Direct base64 transmission; RGB (`f=24`), RGBA (`f=32`), PNG (`f=100`);
  chunking (`m`), optional zlib compression (`o=z`).
- Transmit, transmit-and-display, query, place and delete (`a=t,T,q,p,d`).
- Image IDs, image numbers, placement IDs, replacement, error responses and
  response suppression (`q=1,2`).
- Source rectangles, cell scaling with aspect ratio preservation, pixel offsets,
  cursor movement control, alpha blending and positive/negative z-order.
- Static deletion modes `a/i/n/c/p/q/r/x/y/z`, including uppercase data release.
- Screen scrolling, bounded scrollback and clipping at scrolling margins;
  independent normal/alternate buffers; clear-screen and reset integration.
- Cell and window pixel-size queries (`CSI 14t`, `16t`, `18t`) and PTY pixel sizes,
  reported in physical pixels on HiDPI screens; Qt rendering stays in logical coordinates.
- Limits: 64 MiB per decoded image/upload, 128 MiB image cache per screen,
  1,024 images, 4,096 placements, 10,000 pixels per dimension. Cache pressure
  evicts unplaced images first, then oldest images. PNG dimensions are checked
  before decoding and decompression output is bounded.

## Current limits

This is not the complete Kitty graphics feature set. File/temporary-file/shared
memory transports (`t=f,t,s`), animation actions, Unicode placeholder placements
(`U=1`), and relative placements (`P/Q`) return `ENOTSUP`. Clients requiring these
features need a fallback; automatic fallback is client-dependent.

Resizing the text grid clears placements because the terminal's existing text
reflow does not preserve graphics anchors. Cached image data remains available
for re-placement. Smooth text scrolling is disabled while graphics are displayed
so image and text positions remain synchronized. Graphics do not enter copied
text or text exports.

## Source and license

The Kitty graphics protocol and reference implementation are by **Kovid Goyal
and contributors**. See [third-party acknowledgements](../THIRD_PARTY_NOTICES.md)
for the upstream copyright notice, license and permanent source links.

The wire behavior was developed against the local Kitty checkout at
`24f7369bb6385e06c44c35702a1478b51611510d`, particularly
`docs/graphics-protocol.rst` and `kitty/graphics.c`. Kitty's graphics C source
states GPL3 licensing. Its renderer depends on Kitty's own OpenGL/Python runtime,
so no Kitty source was vendored or linked into this implementation.
`KittyGraphics.{h,cpp}` is an independent Qt implementation under
`GPL-3.0-or-later`, matching GXDE Terminal's application license.

## Verification

The existing GTest executable includes `KittyGraphics.*` tests covering chunked
uploads, decoding, compression, malformed input, IDs, deletion, limits, scrolling,
pixel rendering, fragmented parser input, alternate buffers and image-only
repaints. Build and run:

```sh
cmake --build build -j4
QT_QPA_PLATFORM=offscreen build/tests/deepin-terminal-test --gtest_filter='KittyGraphics.*'
```
