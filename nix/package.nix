{
  stdenv,
  lib,
  cmake,
  pkg-config,
  gettext,
  fmt,
  libpsl,
  cxxopts,
  kdePackages,
}:

stdenv.mkDerivation (finalAttrs: {
  pname = "rtremotesf";
  version = "2.10.1";

  src = lib.cleanSource ../.;

  nativeBuildInputs = [
    cmake
    pkg-config
    gettext
    kdePackages.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.qtbase
    kdePackages.qttools
    fmt
    libpsl
    kdePackages.kwidgetsaddons
    kdePackages.kwindowsystem
    cxxopts
  ];

  meta = {
    description = "Remote GUI for rtorrent";
    mainProgram = "rtremotesf";
    license = lib.licenses.gpl3Plus;
  };
})
