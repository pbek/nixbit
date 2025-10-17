{ lib
, stdenv
, cmake
, ninja
, kdePackages
, qt6
}:

stdenv.mkDerivation {
  pname = "knixosupdater";
  version = "1.0.0";

  src = ./.;

  nativeBuildInputs = [
    cmake
    ninja
    kdePackages.extra-cmake-modules
    qt6.wrapQtAppsHook
  ];

  buildInputs = [
    kdePackages.kcoreaddons
    kdePackages.ki18n
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtwayland
  ];

  preConfigure = ''
    rm -rf build
  '';

  cmakeFlags = [
    "-DCMAKE_BUILD_TYPE=Release"
  ];

  meta = with lib; {
    description = "A KDE Plasma application to display git status in a terminal-like interface";
    homepage = "https://github.com/yourusername/knixosupdater";
    license = licenses.gpl3Plus;
    maintainers = [ ];
    platforms = platforms.linux;
    mainProgram = "knixosupdater";
  };
}
