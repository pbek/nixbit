# libghostty-vt built from the Ghostty 1.4.0 release, as required by Kwi.
# nixpkgs' libghostty-vt still tracks an older development snapshot, so reuse
# the source and Zig dependency cache of nixpkgs' Ghostty package instead.
{
  ghostty,
  libghostty-vt,
  zig_0_16,
  zig_0_17,
}:

libghostty-vt.overrideAttrs (
  finalAttrs: previousAttrs: {
    inherit (ghostty) version src deps;

    nativeBuildInputs = map (
      input: if input == zig_0_16 then zig_0_17 else input
    ) previousAttrs.nativeBuildInputs;

    zigBuildFlags = previousAttrs.zigBuildFlags ++ [
      "-Dversion-string=${finalAttrs.version}"
      "-Demit-themes=false"
      "-Demit-terminfo=false"
      "-Di18n=false"
    ];

    meta = previousAttrs.meta // {
      platforms = zig_0_17.meta.platforms;
    };
  }
)
