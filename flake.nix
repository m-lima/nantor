{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
      in
      {
        devShells = {
          kicad = pkgs.mkShell {
            buildInputs = [
              pkgs.kicad
            ];
          };
          inkscape = pkgs.mkShell {
            buildInputs = [
              pkgs.inkscape
            ];
          };
          zmk = pkgs.mkShell {
            buildInputs = [
              pkgs.yarn
            ];
          };
        };
      }
    );
}
