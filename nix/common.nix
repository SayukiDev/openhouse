# What both packages (built from source, and from the release) share: menu entries, icons and the
# udev rule, and the links on the PATH. The package folder itself (share/openhouse) is laid out
# as the Linux release: the installer, driver_lighthouse.so, apps/, firmwares/.
{ lib, makeDesktopItem, version }:
{
  apps = [ "lhtool" "lhcapture" "lhviz" "lhsession" "openhouse-viewer" "openhouse-overlay" "doorman-dongle-manager" ];

  # After share/openhouse is in place ($share), from the repository at $repo.
  installCommon = ''
    mkdir -p $out/bin
    # On the PATH: links, so each program still finds its package folder from its own path.
    ln -s $share/openhouse-installer $out/bin/openhouse-installer
    ln -s $share/apps/openhouse-overlay $out/bin/openhouse
    for app in $share/apps/*; do
      [ -x "$app" ] && [ -f "$app" ] && [[ "$app" != *.so ]] && ln -s "$app" $out/bin/$(basename "$app")
    done
    # The device access rule (the NixOS module puts it in udev: services.udev.packages).
    install -Dm644 $repo/cmake/linux/60-openhouse.rules $out/lib/udev/rules.d/60-openhouse.rules
    install -Dm644 $repo/cmake/linux/openhouse.png $out/share/icons/hicolor/256x256/apps/openhouse.png
    install -Dm644 $repo/cmake/linux/dongle-manager.png $out/share/icons/hicolor/256x256/apps/doorman-dongle-manager.png
  '';

  desktopItems = [
    (makeDesktopItem {
      name = "openhouse";
      desktopName = "openhouse";
      comment = "Devices, base stations, room and settings";
      exec = "openhouse";
      icon = "openhouse";
      categories = [ "Utility" ];
      # Listed in Envision's plugins, to opt into: the window opens with each session.
      extraConfig = { "X-XR-Plugin" = "true"; };
    })
    (makeDesktopItem {
      name = "doorman-dongle-manager";
      desktopName = "Doorman Dongle Manager";
      comment = "Doorman Dongle pairing, channels and firmware";
      exec = "doorman-dongle-manager";
      icon = "doorman-dongle-manager";
      categories = [ "Utility" ];
    })
    (makeDesktopItem {
      name = "openhouse-installer";
      desktopName = "openhouse installer (v${version})";
      comment = "Installs, repairs or removes openhouse, and flashes its firmware";
      exec = "openhouse-installer";
      icon = "openhouse";
      categories = [ "Utility" ];
    })
  ];

  meta = {
    description = "openhouse: an open lighthouse tracking driver for SteamVR and Monado";
    homepage = "https://github.com/YusufOzmen01/openhouse";
    license = lib.licenses.mit;
    platforms = [ "x86_64-linux" ];
    mainProgram = "openhouse";
  };
}
