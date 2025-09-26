{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";

    src-zephyr = {
      url = "github:zephyrproject-rtos/zephyr/v4.2.0";
      flake = false;
    };
    zephyr-nix = {
      url = "github:nix-community/zephyr-nix";
      inputs = {
        nixpkgs.follows = "nixpkgs";
        zephyr.follows = "src-zephyr";
      };
    };
  };

  outputs =
    {
      nixpkgs,
      flake-utils,
      zephyr-nix,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        zephyr = zephyr-nix.packages.${system};
      in
      {
        devShells = {
          kicad = pkgs.mkShell {
            buildInputs = [ pkgs.kicad ];
          };
          inkscape = pkgs.mkShell {
            buildInputs = [ pkgs.inkscape ];
          };
          zmk =
            let
              policy = pkgs.writeText "policy.json" ''{ "default": [ { "type": "insecureAcceptAnything" } ] }'';
            in
            pkgs.mkShell {
              packages = [
                pkgs.podman
                (pkgs.writeShellScriptBin "podman-build-nix" "podman build --signature-policy ${policy} $@")
              ];
            };
          zmk-dev = pkgs.mkShell {
            packages = [
              (zephyr.sdk.override { targets = [ "arm-zephyr-eabi" ]; })
            ];
          };
        };
      }
    );
}
