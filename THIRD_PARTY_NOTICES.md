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

## Catppuccin colour schemes

The `Catppuccin Latte`, `Catppuccin Frappé`, `Catppuccin Macchiato` and
`Catppuccin Mocha` colour schemes in `3rdparty/terminalwidget/lib/color-schemes/`
use the [Catppuccin](https://github.com/catppuccin/catppuccin) palette, by the
**Catppuccin organisation and contributors**.

Colours are mapped as described in the Terminals section of the Catppuccin
[style guide](https://github.com/catppuccin/catppuccin/blob/d09787dd98ca6fba08af5ef2ae94a7e09f17daca/docs/style-guide.md)
(reference revision `d09787dd98ca6fba08af5ef2ae94a7e09f17daca`); the hex values,
including the generated bright ANSI colours, come from
[catppuccin/palette](https://github.com/catppuccin/palette) v1.8.0.

The upstream project carries this notice:

> Copyright (c) 2021 Catppuccin
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.

## Nord colour scheme

The `Nord` colour scheme in `3rdparty/terminalwidget/lib/color-schemes/` is the
[Nord](https://www.nordtheme.com/docs/colors-and-palettes) palette by
**Sven Greb**, taken from the official Konsole port
[nordtheme/konsole](https://github.com/nordtheme/konsole/blob/fb117d12e2152a7a317927ce1f964f17e791764e/src/nord.colorscheme)
(reference revision `fb117d12e2152a7a317927ce1f964f17e791764e`). The terminal
colours are unchanged; GXDE Terminal adds the tab colours (nord8 for the selected
tab, nord4 for the others).

The upstream project carries this notice:

> Copyright (c) 2016-present Sven Greb <development@svengreb.de> (https://www.svengreb.de)
>
> Permission is hereby granted, free of charge, to any person obtaining a copy
> of this software and associated documentation files (the "Software"), to deal
> in the Software without restriction, including without limitation the rights
> to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
> copies of the Software, and to permit persons to whom the Software is
> furnished to do so, subject to the following conditions:
>
> The above copyright notice and this permission notice shall be included in all
> copies or substantial portions of the Software.
>
> THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
> IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
> FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
> AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
> LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
> OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
> SOFTWARE.
