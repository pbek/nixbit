{
  description = "Nixbit - A KDE Plasma application to display git status";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      self,
      nixpkgs,
      flake-utils,
    }:
    {
      nixosModules = {
        nixbit =
          {
            config,
            lib,
            pkgs,
            ...
          }:

          let
            inherit (lib)
              mkEnableOption
              mkOption
              mkIf
              types
              ;
            cfg = config.services.nixbit;
          in
          {
            options.services.nixbit = {
              enable = mkEnableOption "Nixbit configuration";

              package = mkOption {
                type = types.package;
                default = pkgs.callPackage ./package.nix { };
                description = "The Nixbit package to install";
              };

              repository = mkOption {
                type = types.str;
                description = "Git repository URL for Nixbit";
              };
            };

            config = mkIf cfg.enable {
              environment.systemPackages = [ cfg.package ];

              environment.etc."nixbit.conf".text = ''
                [Repository]
                Url = ${cfg.repository}
              '';
            };
          };
      };
    }
    // flake-utils.lib.eachDefaultSystem (
      system:
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
