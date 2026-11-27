# NixOS: openhouse's programs and the udev rule that lets the logged-in user open lighthouse
# devices and Doorman dongles. On NixOS /etc is built from the configuration, so the installer
# can't add the rule itself; it says to enable this instead.
#   imports = [ openhouse.nixosModules.default ];
#   programs.openhouse.enable = true;
# Then run openhouse-installer to install the driver into SteamVR (it lives in Steam's folder).
self:
{ config, lib, pkgs, ... }:
let
  cfg = config.programs.openhouse;
in
{
  options.programs.openhouse = {
    enable = lib.mkEnableOption "openhouse (its apps and device access)";
    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.stdenv.hostPlatform.system}.default;
      defaultText = lib.literalExpression "openhouse.packages.\${system}.default";
      description = "The openhouse package.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [ cfg.package ];
    services.udev.packages = [ cfg.package ];
  };
}
