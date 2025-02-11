{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };
  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux"];
    forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
    nixpkgsFor = forAllSystems (system: import nixpkgs {inherit system;});
  in {
    packages = forAllSystems (system: let
      pkgs = nixpkgsFor.${system};
    in {
      default = pkgs.stdenv.mkDerivation rec {
        name = "rvm";
        src = ./.;
        outputs = ["out"];

        nativeBuildInputs = with pkgs; [
          meson
          ninja
          gcc
        ];

        enableParallelBuilding = true;
        # meta = with nixpkgs.lib; {
        #   homepage = "https://github.com/RoBaertschi/rvm";
        #   license = with licenses; [mit];
        #   maintainers = ["Robin Bärtschi"];
        # };
      };
    });

    devShells = forAllSystems (system: let
      pkgs = nixpkgsFor.${system};
    in {
      default = pkgs.mkShell {
        buildInputs = with pkgs; [
          meson
          ninja
          gcc
        ];
      };
    });
  };
}
