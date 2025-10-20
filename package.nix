{
  lib,
  stdenv,
  cmake,
  ninja,
  pkg-config,
  kdePackages,
  qt6,
  libgit2,
  installShellFiles,
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

    src = ./.;

    nativeBuildInputs = [
      cmake
      ninja
      pkg-config
      kdePackages.extra-cmake-modules
      qt6.wrapQtAppsHook
      installShellFiles
    ]
    ++ lib.optionals stdenv.hostPlatform.isLinux [ xvfb-run ];

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

    # Install shell completion on Linux (with xvfb-run)
    postInstall =
      lib.optionalString stdenv.hostPlatform.isLinux ''
        installShellCompletion --cmd ${finalAttrs.pname} \
          --bash <(xvfb-run $out/bin/${finalAttrs.pname} --completion-bash) \
          --fish <(xvfb-run $out/bin/${finalAttrs.pname} --completion-fish)
      ''
      # Install shell completion on macOS
      + lib.optionalString stdenv.hostPlatform.isDarwin ''
        installShellCompletion --cmd ${finalAttrs.pname} \
          --bash <($out/bin/${finalAttrs.pname} --completion-bash) \
          --fish <($out/bin/${finalAttrs.pname} --completion-fish)
      '';

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
