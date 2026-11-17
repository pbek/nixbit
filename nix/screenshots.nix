{ pkgs, nixbit }:

pkgs.testers.runNixOSTest {
  name = "nixbit-screenshots";

  nodes.machine = {
    services = {
      xserver.enable = true;
      desktopManager.plasma6.enable = true;
      displayManager = {
        sddm.enable = true;
        autoLogin = {
          enable = true;
          user = "demo";
        };
        defaultSession = "plasma";
      };
    };
    users.users.demo = {
      isNormalUser = true;
      uid = 1000;
      password = "";
    };
    programs.kde-pim.enable = false;
    environment.etc."xdg/kscreenlockerrc".text = ''
      [Daemon]
      Autolock=false
      LockOnResume=false
    '';
    environment.systemPackages = [
      nixbit
      pkgs.python3
      pkgs.kdotool
      pkgs.kdePackages.spectacle
    ];
    virtualisation = {
      memorySize = 4096;
      cores = 2;
      # The resolution option alone only configures X11/GRUB, not Wayland.
      qemu.options =
        pkgs.lib.optionals pkgs.stdenv.hostPlatform.isx86_64 [
          "-vga none"
          "-device virtio-gpu-pci"
        ]
        ++ [
          "-global virtio-gpu-pci.xres=1440"
          "-global virtio-gpu-pci.yres=1080"
        ];
      resolution = {
        x = 1440;
        y = 1080;
      };
    };
    system.stateVersion = "26.05";
  };

  testScript = ''
    # Fresh capture run: ${builtins.getEnv "NIXBIT_SCREENSHOT_RUN"}
    start_all()
    machine.wait_for_unit("display-manager.service")
    machine.wait_until_succeeds("pgrep -u demo plasmashell")
    # SDDM can occupy wayland-0; use the desktop's actual display instead.
    environment_command = "su - demo -c 'env XDG_RUNTIME_DIR=/run/user/1000 systemctl --user show-environment'"
    machine.wait_until_succeeds(environment_command + " | grep '^WAYLAND_DISPLAY='", timeout=120)
    session = dict(line.split("=", 1) for line in machine.succeed(environment_command).splitlines() if "=" in line)
    display = session["WAYLAND_DISPLAY"]
    import re
    assert re.fullmatch(r"wayland-[0-9]+", display), display
    machine.wait_for_file("/run/user/1000/" + display, timeout=120)
    machine.wait_until_succeeds("su - demo -c 'env XDG_RUNTIME_DIR=/run/user/1000 systemctl --user is-active plasma-workspace-wayland.target'", timeout=120)
    machine.succeed("install -d -o demo -g users /home/demo/captures")
    machine.succeed(
        f"su - demo -c 'env XDG_RUNTIME_DIR=/run/user/1000 WAYLAND_DISPLAY={display} "
        "DBUS_SESSION_BUS_ADDRESS=unix:path=/run/user/1000/bus "
        "KDE_SESSION_VERSION=6 XDG_CURRENT_DESKTOP=KDE "
        "QT_QPA_PLATFORMTHEME=kde QT_QUICK_CONTROLS_STYLE=org.kde.desktop "
        "QT_PLUGIN_PATH=${pkgs.kdePackages.plasma-integration}/lib/qt-6/plugins:${pkgs.kdePackages.breeze}/lib/qt-6/plugins "
        "QT_QPA_PLATFORM=wayland QT_QUICK_BACKEND=software "
        "python3 ${../scripts/screenshots.py} --binary ${nixbit}/bin/nixbit "
        "--output /home/demo/captures/screenshots --overview --framed'"
    )
    machine.copy_from_machine("/home/demo/captures/screenshots", "")
  '';
}
