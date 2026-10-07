# Third-party acknowledgements

## Kitty graphics protocol

GXDE Terminal's Kitty graphics support refers to the protocol and implementation
of [kitty](https://github.com/kovidgoyal/kitty), by **Kovid Goyal and contributors**.

Reference revision: `24f7369bb6385e06c44c35702a1478b51611510d`.

Reference files:

- [docs/graphics-protocol.rst](https://github.com/kovidgoyal/kitty/blob/24f7369bb6385e06c44c35702a1478b51611510d/docs/graphics-protocol.rst)
- [kitty/graphics.c](https://github.com/kovidgoyal/kitty/blob/24f7369bb6385e06c44c35702a1478b51611510d/kitty/graphics.c)
- [kitty/graphics.h](https://github.com/kovidgoyal/kitty/blob/24f7369bb6385e06c44c35702a1478b51611510d/kitty/graphics.h)

The upstream graphics source carries this notice:

> Copyright (C) 2017 Kovid Goyal <kovid at kovidgoyal.net>
>
> Distributed under terms of the GPL3 license.

The upstream license text is available in
[kitty's LICENSE](https://github.com/kovidgoyal/kitty/blob/24f7369bb6385e06c44c35702a1478b51611510d/LICENSE).
GXDE Terminal also includes the GNU GPL version 3 text in its root `LICENSE`.

`3rdparty/terminalwidget/lib/KittyGraphics.{h,cpp}` is a separately written Qt
implementation, licensed GPL-3.0-or-later. No Kitty source files are vendored or
linked into it. This acknowledgement credits the upstream protocol and reference
implementation; it does not attribute authorship of the new Qt code to upstream.
