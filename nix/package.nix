{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  qt6,
  lxqt,
  kdePackages,
  dtk6widget,
  glib,
  libsecret,
  libX11,
  libxcb-wm,
  libchardet,
  libuchardet,
  icu,
  zlib,
  expect,
  zssh,
  openssh,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "gxde-terminal";
  version = lib.removeSuffix ")" (lib.removePrefix "(" (
    builtins.elemAt (lib.splitString " " (builtins.head (
      lib.splitString "\n" (builtins.readFile ../debian/changelog)
    ))) 1
  ));

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../cmake
      ../src
      ../3rdparty
      ../translations
      ../tests
      ../LICENSE
      ../THIRD_PARTY_NOTICES.md
    ];
  };

  nativeBuildInputs = [ cmake ninja pkg-config qt6.qttools qt6.wrapQtAppsHook lxqt.lxqt-build-tools ];
  buildInputs = [
    qt6.qtbase
    qt6.qt5compat
    qt6.qtsvg
    qt6.qtwayland
    kdePackages.kwindowsystem
    dtk6widget
    glib
    libsecret
    libX11
    libxcb-wm
    libchardet
    libuchardet
    icu
    zlib
  ];

  cmakeBuildType = "Release";
  cmakeFlags = [
    (lib.cmakeFeature "VERSION" finalAttrs.version)
    (lib.cmakeFeature "APP_VERSION" finalAttrs.version)
    (lib.cmakeBool "TERM_RPATH" false)
    (lib.cmakeBool "INSTALL_USER_MANUAL" false)
    (lib.cmakeBool "RUNDIR_KEYBOARD_LAYOUT_FIRST" false)
  ];

  # Remote sessions invoke these tools from the shell, not by absolute path.
  qtWrapperArgs = [ "--suffix" "PATH" ":" (lib.makeBinPath [ expect zssh openssh ]) ];

  postInstall = ''
    install -Dm644 ${../LICENSE} "$out/share/licenses/gxde-terminal/LICENSE"
  '';

  doInstallCheck = true;
  installCheckPhase = ''
    runHook preInstallCheck
    test -x "$out/bin/gxde-terminal"
    test -f "$out/share/applications/gxde-terminal.desktop"
    test -f "$out/share/doc/gxde-terminal/THIRD_PARTY_NOTICES.md"
    test -d "$out/share/gxde-terminalwidget6/kb-layouts"
    test -d "$out/share/gxde-terminal/translations"
    runHook postInstallCheck
  '';

  meta = {
    description = "GXDE terminal emulator with tabs, split panes and Kitty graphics";
    homepage = "https://github.com/GXDE-OS/gxde-terminal";
    license = lib.licenses.gpl3Plus;
    platforms = lib.platforms.linux;
    mainProgram = "gxde-terminal";
  };
})
