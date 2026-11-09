{
  lib,
  stdenv,
  fetchurl,
  fetchzip,
  ncurses,
  pkg-config,
  zig_0_17,
}:

# Match the released VT API used by upstream Kwi, independently of the Ghostty app.
let
  version = "1.0.0";
  deps = fetchzip {
    url = "https://release.files.ghostty.org/libghostty-vt/${version}/libghostty-vt-${version}-deps.tar.gz";
    hash = "sha256-bqi6TjqxfMtLYyStfHeqZBPsapfdJHVT3ILxL7u+kEQ=";
  };
in
stdenv.mkDerivation {
  pname = "libghostty-vt";
  inherit version;
  src = fetchurl {
    url = "https://release.files.ghostty.org/libghostty-vt/${version}/libghostty-vt-${version}.tar.gz";
    sha256 = "4a6ef758be721923e1969e7b81b443dcc92d8dd3fb5a39600c03a8817a753676";
  };

  strictDeps = true;
  nativeBuildInputs = [
    ncurses
    pkg-config
    zig_0_17
  ];

  dontSetZigDefaultFlags = true;
  zigBuildFlags = [
    "--system"
    "${deps}"
    "-Dlib-version-string=${version}"
    "-Dcpu=baseline"
    "-Doptimize=ReleaseFast"
    "-Dapp-runtime=none"
    "-Demit-lib-vt=true"
    "-Demit-themes=false"
    "-Demit-terminfo=false"
    "-Di18n=false"
  ];

  outputs = [
    "out"
    "dev"
  ];

  postInstall = ''
    mkdir -p "$dev/lib"
    mv "$out/lib/libghostty-vt.a" "$dev/lib/"
  '';

  postFixup = ''
    substituteInPlace "$dev/lib/pkgconfig/libghostty-vt-static.pc" \
      --replace-fail "$out" "$dev"
    substituteInPlace "$dev/lib/cmake/ghostty-vt/ghostty-vt-config.cmake" \
      --replace-fail 'set(_ghostty_vt_shared_libdir "''${_ghostty_vt_libdir}")' \
        "set(_ghostty_vt_shared_libdir \"$out/lib\")"
  '';

  meta = {
    description = "Ghostty's terminal parsing and state library for Kwi";
    homepage = "https://ghostty.org/";
    license = lib.licenses.mit;
    platforms = lib.platforms.linux;
  };
}
