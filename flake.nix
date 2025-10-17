{
  description = "KNixOS Updater - A KDE Plasma application to display git status";

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
          knixosupdater = pkgs.callPackage ./package.nix { };
          default = self.packages.${system}.knixosupdater;
        };

        apps = {
          knixosupdater = {
            type = "app";
            program = "${self.packages.${system}.knixosupdater}/bin/knixosupdater";
          };
          default = self.apps.${system}.knixosupdater;
        };

        devShells.default = pkgs.mkShell {
          inputsFrom = [ self.packages.${system}.knixosupdater ];
          packages = with pkgs; [
            just
            git
          ];
        };
      }
    );
}

