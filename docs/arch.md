# Arch Linux packaging

On an up-to-date Arch Linux x86_64 system:

```sh
sudo pacman -Syu --needed base-devel
./build-arch
```

The helper generates a source archive and a PKGBUILD with its SHA-256 checksum,
then runs `makepkg --syncdeps` as the current user. Pacman may request sudo access
to install missing build dependencies. The resulting `*.pkg.tar.zst` files are
written to `build-arch-output/`; the generated PKGBUILD, `.SRCINFO` and build tree
remain in a uniquely named `work.*` subdirectory.

To also install the package:

```sh
./build-arch -- --install
```

To prepare sources on another Linux distribution, without Arch tools:

```sh
./build-arch --prepare-only --output-dir /tmp/gxde-arch
```

Transfer the printed `work.*` directory to Arch, enter it, and run
`makepkg --syncdeps`. `arch/PKGBUILD.in` is a template; the generated `PKGBUILD`
is the file consumed by makepkg. Source checksums are real SHA-256 hashes, not
`SKIP`. `--output-dir` works for full builds too; arguments after `--` are passed
to makepkg. Set `CMAKE_BUILD_PARALLEL_LEVEL` to limit compile parallelism.

The source snapshot includes local edits to the application. It excludes Git
metadata, build directories and generated translation binaries. The package
version comes from `debian/changelog`; `SOURCE_DATE_EPOCH` defaults to the last
commit timestamp and can be supplied explicitly. Preparing the source archive
does not install anything or require root.

The package uses Qt 6, DTK6, KDE WindowSystem, ICU, zlib and the encoding libraries
from Arch's repositories. See the official
[DTK6 widget package](https://archlinux.org/packages/extra/x86_64/dtk6widget/) and
[Deepin Terminal dependency list](https://archlinux.org/packages/extra/x86_64/deepin-terminal/)
for the corresponding upstream Arch package names. `qt6-wayland` is optional for
native Wayland support; `expect`, `zssh` and `openssh` enable remote sessions.

The executable, desktop entry and widget library use GXDE-specific names and do
not replace Deepin Terminal. The package includes licenses and Kitty attribution.
The offline user manual and the Debug-only GUI unit test executable are not built.
This is a local packaging workflow, not an AUR submission or a published release.
