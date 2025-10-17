{
  description = "Nixbit - A KDE Plasma application to display git status";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        packages = {
          nixbit = pkgs.callPackage ./package.nix { };
          default = self.packages.${system}.nixbit;
        };

        apps = {
          nixbit = {
            type = "app";
            program = "${self.packages.${system}.nixbit}/bin/nixbit";
          };
          default = self.apps.${system}.nixbit;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.nixbit ];
          packages = with pkgs; [
            just
            git
          ];
        };
      }
    );
}
