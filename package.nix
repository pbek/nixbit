{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  kdePackages,
  qt6,
  libgit2,
}:

stdenv.mkDerivation (
  _finalAttrs:
  let
    # Extract version from CMakeLists.txt
    cmakeListsContent = builtins.readFile ./CMakeLists.txt;
    versionMatch = builtins.match ".*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*" cmakeListsContent;
    version = if versionMatch != null then builtins.head versionMatch else "0.0.0";
  in
  {
    pname = "nixbit";
    inherit version;

    src = ./.;

    nativeBuildInputs = [
      cmake
      ninja
      pkg-config
      kdePackages.extra-cmake-modules
      qt6.wrapQtAppsHook
    ];

    buildInputs = [
      kdePackages.kcoreaddons
      kdePackages.ki18n
      kdePackages.kconfig
      kdePackages.kirigami
      qt6.qtbase
      qt6.qtdeclarative
      qt6.qtwayland
      libgit2
    ];

    preConfigure = ''
      rm -rf build
    '';

    cmakeFlags = [
      "-DCMAKE_BUILD_TYPE=Release"
    ];

    meta = with lib; {
      description = "A KDE Plasma application to update your nixos system from a git repository";
      homepage = "https://github.com/pbek/nixbit";
      license = licenses.gpl3Plus;
      maintainers = [ "pbek" ];
      platforms = platforms.linux;
      mainProgram = "nixbit";
    };
  }
)
