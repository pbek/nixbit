{ pkgs, lib, ... }:

{
  # Development dependencies
  languages.cplusplus.enable = true;
  languages.nix.enable = true;

  packages = with pkgs; [
    ninja
    git
    just
    pkg-config
    libgit2
    libssh2
    # KDE Frameworks and Qt6
    kdePackages.extra-cmake-modules
    kdePackages.kcoreaddons
    kdePackages.ki18n
    kdePackages.kirigami
    qt6.qtbase
    qt6.qtdeclarative
    qt6.qtwayland
    kdePackages.wrapQtAppsHook
  ];

  env = {
    QT_PLUGIN_PATH = lib.makeSearchPath "lib/qt-6/plugins" [
      pkgs.qt6.qtbase
      pkgs.qt6.qtdeclarative
    ];
    QML_IMPORT_PATH = lib.makeSearchPath "lib/qt-6/qml" [
      pkgs.qt6.qtdeclarative
    ];
    QT_QPA_PLATFORM = "xcb";
  };

  enterShell = ''
    echo "🚀 Nixbit NixOS Updater Development Environment"
    echo "==============================================="
    echo "Available commands:"
    echo "  🔨 just build  - Build the application"
    echo "  ▶️ just run    - Build and run the application"
    echo "  🧹 just clean  - Clean build artifacts"
    echo ""
    echo "🎨 Qt Version: ${pkgs.qt6.qtbase.version}"
    echo "📁 QML_IMPORT_PATH: $QML_IMPORT_PATH"
  '';

  git-hooks = {
    hooks = {
    };
  };
}
