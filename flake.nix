{
  description = "GXDE Terminal with Qt 6 and Kitty graphics support";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    gxde-dtk6 = {
      url = "github:gxde-nix/gxde-dtk6";
      inputs.nixpkgs.follows = "nixpkgs";
    };
  };

  outputs = { self, nixpkgs, gxde-dtk6 }:
    let
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
      packageFor = pkgs: pkgs.callPackage ./nix/package.nix {
        inherit ((import gxde-dtk6 { inherit pkgs; })) dtk6widget;
      };
    in
    {
      overlays.default = final: prev: {
        gxde-terminal = packageFor final;
      };

      packages = forAllSystems (system:
        let package = packageFor nixpkgs.legacyPackages.${system};
        in { default = package; gxde-terminal = package; });

      apps = forAllSystems (system: {
        default = {
          type = "app";
          program = "${self.packages.${system}.default}/bin/gxde-terminal";
          meta.description = "GXDE Terminal";
        };
      });

      devShells = forAllSystems (system:
        let pkgs = nixpkgs.legacyPackages.${system};
        in {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.default ];
            packages = [ pkgs.gdb pkgs.gtest pkgs.qt6.qttools ];
          };
        });

      checks = forAllSystems (system: {
        package = self.packages.${system}.default;
      });
    };
}
