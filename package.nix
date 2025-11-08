{
  cmake,
  installShellFiles,
  kdePackages,
  lib,
  libgit2,
  ninja,
  pkg-config,
  qt6,
  stdenv,
  xvfb-run,
}:

stdenv.mkDerivation (
  finalAttrs:
  let
    # Extract version from CMakeLists.txt
    cmakeListsContent = builtins.readFile ./CMakeLists.txt;
    versionMatch = builtins.match ".*VERSION ([0-9]+\.[0-9]+\.[0-9]+).*" cmakeListsContent;
    version = if versionMatch != null then builtins.head versionMatch else "0.0.0";
  in
  {
    pname = "nixbit";
    inherit version;

    src = lib.cleanSourceWith {
      src = ./.;
      filter =
        path: _type:
        let
          baseName = baseNameOf path;
        in
        !(builtins.elem baseName [
          "build"
          "cmake-build-debug"
          "result"
          ".git"
          ".gitignore"
          ".cache"
        ]);
    };

    nativeBuildInputs = [
      cmake
      installShellFiles
      kdePackages.extra-cmake-modules
      ninja
      pkg-config
      qt6.wrapQtAppsHook
    ]
    ++ lib.optionals stdenv.hostPlatform.isLinux [ xvfb-run ];

    buildInputs = [
      kdePackages.kconfig
      kdePackages.kcoreaddons
      kdePackages.ki18n
      kdePackages.kirigami
      kdePackages.konsole
      kdePackages.kparts
      kdePackages.kservice
      libgit2
      qt6.qtbase
      qt6.qtdeclarative
      qt6.qtwayland
    ];

    # Install shell completion on Linux (with xvfb-run)
    postInstall = ''
      installShellCompletion --cmd nixbit \
        --bash <(xvfb-run $out/bin/nixbit --completion-bash) \
        --fish <(xvfb-run $out/bin/nixbit --completion-fish)
    '';

    meta = with lib; {
      description = "KDE Plasma application to update your NixOS system from a git repository";
      homepage = "https://github.com/pbek/nixbit";
      changelog = "https://github.com/pbek/nixbit/releases/tag/v${finalAttrs.version}";
      license = licenses.gpl3Plus;
      maintainers = with lib.maintainers; [ pbek ];
      platforms = platforms.linux;
      mainProgram = "nixbit";
    };
  }
)
