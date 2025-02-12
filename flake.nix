{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs?ref=nixos-unstable";
  };
  outputs = {
    self,
    nixpkgs,
  }: let
    supportedSystems = ["x86_64-linux" "aarch64-darwin"];
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
          clang
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
      default = with pkgs; let 
          llvm = llvmPackages_latest;
          in
          pkgs.mkShell.override { 
          stdenv = clangStdenv;
        } {
        buildInputs = with pkgs; [
          llvm.libcxx
          llvm.libllvm

          clang-tools
          meson
          ninja
          clang
        ];
        shellHook = ''
          PATH="${pkgs.clang-tools}/bin:$PATH"
        '';
      };
    });
  };
}
