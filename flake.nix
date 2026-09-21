{
  description = "7zQ - a Qt port of the 7-Zip file manager";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      # The port builds on 7-Zip's posix layer, so Linux only. The engine
      # itself has no x86-specific code that is not already guarded, and the
      # assembly is not used by this build.
      systems = [ "x86_64-linux" "aarch64-linux" ];

      forAllSystems = f:
        nixpkgs.lib.genAttrs systems (system: f {
          inherit system;
          pkgs = nixpkgs.legacyPackages.${system};
        });
    in
    {
      # The package definition is shared with default.nix, so nix-build and
      # nix build produce the same derivation.
      packages = forAllSystems ({ pkgs, system, ... }: rec {
        sevenzip-qt = import ./default.nix { inherit pkgs; };
        default = sevenzip-qt;
      });

      apps = forAllSystems ({ system, ... }: rec {
        sevenzip-qt = {
          type = "app";
          program = "${self.packages.${system}.sevenzip-qt}/bin/7zQ";
          meta.description = "7-Zip File Manager (Qt)";
        };
        default = sevenzip-qt;
      });

      # nix develop -> the toolchain, then ./build-qt.sh
      devShells = forAllSystems ({ pkgs, ... }: {
        default = import ./shell.nix { inherit pkgs; };
      });

      overlays.default = final: _prev: {
        sevenzip-qt = import ./default.nix { pkgs = final; };
      };

      checks = forAllSystems ({ system, ... }: {
        build = self.packages.${system}.sevenzip-qt;
      });

      formatter = forAllSystems ({ pkgs, ... }: pkgs.nixpkgs-fmt);
    };
}
