{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-25.05";
    flake-utils.url = "github:numtide/flake-utils";

    src-zephyr = {
      url = "github:zephyrproject-rtos/zephyr/v4.2.0";
      flake = false;
    };
    src-zmk = {
      url = "github:zmkfirmware/zmk/v0.3.0";
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
      src-zmk,
      zephyr-nix,
      ...
    }:
    flake-utils.lib.eachDefaultSystem (
      system:
      let
        pkgs = nixpkgs.legacyPackages.${system};
        zephyr = zephyr-nix.packages.${system};
        zephyr-pkgs = [
          (zephyr.sdk.override {
            targets = [
              "arm-zephyr-eabi"
            ];
          })
          zephyr.pythonEnv
          zephyr.hosttools-nix
        ];
      in
      {
        packages = rec {
          zmk = pkgs.stdenv.mkDerivation {
            pname = "zmk";
            version = "v0.3.0";

            src = src-zmk;

            nativeBuildInputs = zephyr-pkgs ++ [
              pkgs.gitMinimal
              pkgs.cacert
            ];

            configurePhase = ''
              west init -l app
              west update
            '';

            installPhase = ''
              cp -pr --reflink=auto -- . "$out"
            '';
          };

          nantor = pkgs.stdenvNoCC.mkDerivation {
            pname = "nantor";
            version = "0.1";

            dontUseCmakeConfigure = true;
            dontUseNinjaBuild = true;
            dontUseHostLibc = true;

            src = ./zmk/shared;

            nativeBuildInputs = zephyr-pkgs ++ [
              pkgs.cmake
              pkgs.gitMinimal
              pkgs.ninja
            ];

            configurePhase = ''
              cp -r "${zmk}" zmk
              # find zmk -perm 444 -exec chmod 644 {} +
              # find zmk -perm 555 -exec chmod 755 {} +
              mkdir -p "build"
              cmake -DWEST_PYTHON="${zephyr.pythonEnv}/bin/python3.12" -B"build/left" -GNinja -DBOARD=pillbug -DSHIELD=nantor_left -DZMK_CONFIG="$src/config" -DZMK_EXTRA_MODULES="$src/modules/status_led" -S"zmk/app"
              cmake -DWEST_PYTHON="${zephyr.pythonEnv}/bin/python3.12" -B"build/right" -GNinja -DBOARD=pillbug -DSHIELD=nantor_right -DZMK_CONFIG="$src/config" -DZMK_EXTRA_MODULES="$src/modules/status_led" -S"zmk/app"
            '';

            buildPhase = ''
              cd build_left
              ninja
            '';

            # installPhase =
            #   let
            #     buildCmd =
            #       side:
            #       ''west build -s app --pristine -d $out/${side} -b pillbug -- -DSHIELD=nantor_${side} -DZMK_CONFIG=$src/config -DZMK_EXTRA_MODULES=$src/modules/status_led'';
            #   in
            #   ''
            #     mkdir -p "$out"
            #     cd zmk
            #     # west zephyr-export
            #     echo "BASE: $ZEPHYR_BASE"
            #     echo "SDK $ZEPHYR_SDK_INSTALL_DIR"
            #     ${buildCmd "left"}
            #     ${buildCmd "right"}
            #   '';
          };
        };

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
            packages = zephyr-pkgs ++ [
              pkgs.cmake
              pkgs.ninja
            ];
          };
        };
      }
    );
}
