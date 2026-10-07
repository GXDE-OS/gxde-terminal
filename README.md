<!-- Improved compatibility of back to top link: See: https://github.com/GXDE-OS/gxde-terminal/pull/73 -->
<a id="readme-top"></a>
<!--
*** Thanks for checking out the Best-README-Template. If you have a suggestion
*** that would make this better, please fork the repo and create a pull request
*** or simply open an issue with the tag "enhancement".
*** Don't forget to give the project a star!
*** Thanks again! Now go create something AMAZING! :D
-->



<!-- PROJECT SHIELDS -->
<!--
*** I'm using markdown "reference style" links for readability.
*** Reference links are enclosed in brackets [ ] instead of parentheses ( ).
*** See the bottom of this document for the declaration of the reference variables
*** for contributors-url, forks-url, etc. This is an optional, concise syntax you may use.
*** https://www.markdownguide.org/basic-syntax/#reference-style-links
-->
[![Contributors][contributors-shield]][contributors-url]
[![Forks][forks-shield]][forks-url]
[![Stargazers][stars-shield]][stars-url]
[![Issues][issues-shield]][issues-url]
[![License][license-shield]][license-url]



<!-- PROJECT LOGO -->
<br />
<div align="center">
  <a href="https://github.com/GXDE-OS/gxde-terminal">
    <img src="src/assets/logo/gxde-title.svg" alt="Logo" width="80" height="80">
  </a>

  <h3 align="center">GXDE Terminal</h3>

  <p align="center">
    A fork of Deepin terminal with the interface from the good old days.
    <br />
    <a href="https://gxde.top/en/"><strong>Explore GXDE »</strong></a>
    <br />
    <br />
    <a href="https://github.com/GXDE-OS/gxde-terminal/actions/workflows/launcher-building.yml">View CI status</a>
    &middot;
    <a href="https://gitee.com/GXDE-OS/gxde-terminal/issues">Report Bug</a>
    &middot;
    <a href="https://gitee.com/GXDE-OS/gxde-terminal/issues">Request Feature</a>
  </p>
</div>



<!-- ABOUT THE PROJECT -->
## About The Project

![Screenshot](./docs/img/screenshot.png)

GXDE Terminal is a fork of Deepin terminal, which is "an advanced terminal emulator with workspace , multiple windows, remote management, quake mode and other features."

GXDE Terminal is now built upon Deepin DTK6, yet it deliberately emulates the UI style and operational logic of `deepin-terminal-gtk`.



### Built With

- qt6-5compat-dev
- qt6-base-dev
- qt6-tools-dev
- qt6-tools-dev-tools
- qt6-base-private-dev
- libdtk6widget-dev
- libkf6windowsystem-dev
- lxqt-build-tools
- libutf8proc-dev
- libglib2.0-dev
- libsecret-1-dev
- libgtest-dev
- libgmock-dev
- libxcb-ewmh-dev
- libchardet-dev
- libuchardet-dev
- libicu-dev
- zlib1g-dev
- expect
- zssh
- libuchardet0
- libchardet1



<!-- GETTING STARTED -->
## Getting Started
### Manually Build & Install w/ CMake
#### Prerequisites

Make sure you have installed all dependencies, you can use the following command：

```shell
$ cd gxde-terminal
$ sudo apt build-dep .
```

#### Build
You will need to have CMake ready, then:
```shell
$ cd gxde-terminal
$ mkdir build
$ cd build
$ cmake ..
$ make
```

#### Installation
```shell
$ sudo make install
```

The executable binary file could be found at /usr/bin/gxde-terminal after the installation is finished.

### Packaging
#### Debian
We've provided you a script to automatically install the dependency, build the terminal, and pack the terminal for distributions using dpkg/apt as package manager.

```shell
$ chmod a+x ./build-deb  # Make sure that the permission is correct.
$ ./build-deb -d         # -d parameter will automatically trigger dependency discover and installation, and for second time building ./build-deb without -d is fine.
```

You may find the artifact(s) on the parent folder.

Once you're finished, you may wish run the following command to do the clean-ups:
```shell
$ ./build-deb -c
```

Note that this will also delete those artifact(s) so be sure to back up them if you need.

#### Nix
We also have a Nix building script:
```shell
$ chmod a+x ./build-nix  # Handle permission issue
$ ./build-nix            # Start building
$ nix profile add .#gxde-terminal # Installation
```

#### Arch
We also have a Arch `.pkg.tar.zst` building script:
```shell
$ chmod a+x ./build-arch  # Handle permission issue
$ ./build-arch            # Start building
```


<!-- USAGE EXAMPLES -->
## Usage
There is nothing special for GXDE terminal, you may use it in the same way as you use Deepin terminal, most features are straight forward, expect that we have following modifications:
- Font
  - We provide a fallback font option in the *Settings* section.
  - You may also enable non-monospaced fonts, but that's for fallback font only.
  - Font ligature is avaliable in the same section now.
- **Graphics**: Now GXDE terminal support Kitty's graphics protocol.


<!-- ROADMAP -->
## Roadmap
- [x] Implement DTK2 styled UI.
- [x] Add UI animation.
- [x] Add fallback font system.
- [x] Add support for font ligature.
- [x] Add support for Kitty graphics protocol.
- [x] Packaging
    - [x] Debian.
    - [x] Arch.
    - [x] Nix.

See the [open issues](https://gitee.com/GXDE-OS/gxde-terminal/issues) for a full list of proposed features (and known issues).



<!-- CONTRIBUTING -->
## Contributing
### Top contributors:

<a href="https://github.com/GXDE-OS/gxde-terminal/graphs/contributors">
  <img src="https://contrib.rocks/image?repo=GXDE-OS/gxde-terminal" alt="contrib.rocks image" />
</a>



<!-- LICENSE -->
## License
- *GXDE Terminal* is licensed under *[GNU GENERAL PUBLIC LICENSE Version 3](./LICENSE)*.
- You may also want to read our *[Third Party Notices](./THIRD_PARTY_NOTICES.md)*.
- For REUSE information you may want to see the *[dep5](./.reuse/dep5)* file.


<!-- CONTACT -->
## Contact
Please post an issue if you have any questions or concerns.



<!-- ACKNOWLEDGMENTS -->
## Acknowledgments
- **deepin-terminal**: https://github.com/linuxdeepin/deepin-terminal
- **deepin-terminal-gtk**: https://github.com/martyr-deepin/deepin-terminal-gtk
- **qterminalwidget**: https://github.com/lxqt/qtermwidget
- **kitty**: https://github.com/kovidgoyal/kitty
- **best-readme-template**: https://github.com/othneildrew/Best-README-Template



<!-- MARKDOWN LINKS & IMAGES -->
<!-- https://www.markdownguide.org/basic-syntax/#reference-style-links -->
[contributors-shield]: https://img.shields.io/github/contributors/GXDE-OS/gxde-terminal.svg?style=plastic
[contributors-url]: https://github.com/GXDE-OS/gxde-terminal/graphs/contributors
[forks-shield]: https://img.shields.io/github/forks/GXDE-OS/gxde-terminal.svg?style=plastic
[forks-url]: https://github.com/GXDE-OS/gxde-terminal/network/members
[stars-shield]: https://img.shields.io/github/stars/GXDE-OS/gxde-terminal.svg?style=plastic
[stars-url]: https://github.com/GXDE-OS/gxde-terminal/stargazers
[issues-shield]: https://img.shields.io/github/issues/GXDE-OS/gxde-terminal.svg?style=plastic
[issues-url]: https://github.com/GXDE-OS/gxde-terminal/issues
[license-shield]: https://img.shields.io/github/license/GXDE-OS/gxde-terminal.svg?style=plastic
[license-url]: https://github.com/GXDE-OS/gxde-terminal/blob/dtk6/LICENSE
