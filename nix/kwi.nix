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
  version = "0-unstable-2026-11-01";

  src = fetchFromGitLab {
    domain = "invent.kde.org";
    owner = "utilities";
    repo = "kwi";
    rev = "483d718d779c069a28ec139280d1d6e5e198b94e";
    hash = "sha256-BA3Rxm7QzSM6SDb35Q7SlKzVAgvYDkHF1IWaZCMHp64=";
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

  extraCmakeFlags = [ "-DBUILD_KPART=OFF" ];

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
