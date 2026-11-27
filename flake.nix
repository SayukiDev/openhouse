{
  description = "openhouse: an open lighthouse tracking driver for SteamVR and Monado";

  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";   # SteamVR's driver is x86-64 only
      pkgs = nixpkgs.legacyPackages.${system};
      lib = pkgs.lib;
      # include/lh/version.hpp's base version, and the commit like a git build stamps it.
      version = builtins.head (builtins.match ''.*#define LH_VERSION_BASE "([^"]*)".*''
        (builtins.readFile ./include/lh/version.hpp));
      commit = self.shortRev or self.dirtyShortRev or "";
      # The repository without build output (only tracked files in a git checkout anyway).
      src = lib.cleanSourceWith {
        src = ./.;
        filter = path: type:
          let name = baseNameOf path; in
          !(lib.hasPrefix "build" name && type == "directory")
          && !(builtins.elem name [ "dist" "captures" "result" ]);
      };
      # The latest release's binaries (nix/release.json, scripts/bump-nix-release.sh): no compiling.
      bin = final: final.callPackage ./nix/package-bin.nix { repo = src; };
      # Built from this checkout: master or a test build.
      source = final: final.callPackage ./nix/package.nix { inherit version commit src; };
    in
    {
      packages.${system} = rec {
        default = openhouse;
        openhouse = bin pkgs;
        openhouse-source = source pkgs;
        # The PC side only, without building the firmware (quicker).
        openhouse-source-no-firmware = openhouse-source.override { withFirmware = false; };
      };

      overlays.default = final: prev: {
        openhouse = bin final;
        openhouse-source = source final;
      };

      nixosModules.default = import ./nix/module.nix self;

      apps.${system} =
        let
          app = program: { type = "app"; program = "${self.packages.${system}.default}/bin/${program}"; };
        in
        {
          default = app "openhouse";
          installer = app "openhouse-installer";
          dongle-manager = app "doorman-dongle-manager";
          lhtool = app "lhtool";
          lhcapture = app "lhcapture";
        };
    };
}
