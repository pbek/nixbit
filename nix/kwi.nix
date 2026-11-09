# Kwi terminal (https://invent.kde.org/utilities/kwi), which provides the
# org.kde.kwi.terminal QML module Nixbit uses for its embedded terminal.
# Kwi has no releases yet, so a recent commit is pinned.
{
  callPackage,
  fetchFromGitLab,
  kdePackages,
  lib,
  pkg-config,
  qt6,
}:

let
  libghostty-vt = callPackage ./libghostty-vt.nix { };
in
kdePackages.mkKdeDerivation {
  pname = "kwi";
  version = "0.1.0-unstable-2026-11-08";

  src = fetchFromGitLab {
    domain = "invent.kde.org";
    owner = "utilities";
    repo = "kwi";
    rev = "1df87417056b4b44085845b7fd273fe95bb0e42f";
    hash = "sha256-I7DjwSTC7kV3H0XokmC0aeGI6aQQ3vq7cOUkkbgQ+1Q=";
  };

  extraNativeBuildInputs = [
    kdePackages.extra-cmake-modules
    pkg-config
    qt6.qtshadertools
  ];

  extraBuildInputs = [
    kdePackages.kconfig
    kdePackages.kcoreaddons
    kdePackages.kdbusaddons
    kdePackages.kguiaddons
    kdePackages.kio
    kdePackages.kirigami-addons
    kdePackages.kirigami-app-components
    kdePackages.kitemmodels
    kdePackages.knotifications
    kdePackages.kpty
    kdePackages.kwindowsystem
    kdePackages.qqc2-desktop-style
    libghostty-vt
    qt6.qtdeclarative
    qt6.qtsvg
    qt6.qtwayland
  ];

  # QML modules imported by the org.kde.kwi.terminal module at runtime
  extraPropagatedBuildInputs = [
    kdePackages.ki18n
    kdePackages.kirigami
  ];

  extraCmakeFlags = [
    "-DBUILD_KPART=OFF"
    "-DBUILD_TESTING=OFF"
  ];

  passthru = { inherit libghostty-vt; };

  meta = {
    description = "Modern and fast terminal for KDE Plasma and other desktops";
    homepage = "https://invent.kde.org/utilities/kwi";
    license = with lib.licenses; [
      gpl2Only
      gpl3Only
      lgpl21Only
      lgpl3Only
    ];
    platforms = lib.platforms.linux;
    mainProgram = "kwi";
  };
}
