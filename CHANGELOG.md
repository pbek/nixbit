# Nixbit Changelog

## 0.3.0

- Added NixOS generations display and management
  - Shows current generation number and date in main UI
  - Compact display that hides during builds to avoid clutter
  - "View All Generations" button opens a dialog with full generation history
  - "Refresh" button to manually update generation list
  - Automatically refreshes after "switch" operations (when new generation is created)
  - Generations list shows generation number, timestamp, and highlights current generation
  - Uses system profile links for reliable generation tracking
- Added comprehensive build host management system in Settings Dialog
  - Create, update, and delete build host configurations with name and address
  - Each build host entry stores a friendly name and SSH address (e.g., user@hostname)
  - Build host list displays in a scrollable view with inline editing
- Added build host selector in main UI next to Rebuild Mode selector
  - Displays "(local)" option plus all configured build hosts
  - Each rebuild mode (build/switch) independently remembers its selected build host
  - Selections persist across application restarts
  - Build host selection automatically restores when switching between modes
- Improved Settings Dialog layout
  - Increased dialog size for better readability (580x460)
  - Build host fields now properly aligned
  - Update and Remove buttons converted to icon-only ToolButtons to save space
  - Address fields expand to fill available space
- Build commands now use the selected build host based on current mode
  - Empty or "(local)" selection results in local builds
  - Named host selections use the configured address for `--build-host` parameter
- Added disk I/O monitoring to system resources display
  - Shows cumulative read/write rates for all physical disks during builds
  - Supports NVMe, SATA, SCSI, virtio, IDE, and MMC/SD block devices
  - Uses `/sys/block` for reliable disk statistics with `/proc/diskstats` fallback
  - Displays real-time I/O rates in human-readable format (B/s, KB/s, MB/s, GB/s)
- Added descriptive label below Rebuild Mode selector explaining the difference between modes
  - Dynamically shows explanation for "build" mode (tests configuration without applying changes, no sudo required)
  - Dynamically shows explanation for "switch" mode (builds and activates new configuration, requires sudo)
  - Helps users understand the purpose of each rebuild mode before executing

## 0.2.4

- The autostart setting now will set `nixbit` as exec path instead of the full
  path to the binary, to allow updates of the application without breaking
  the autostart entry (for [#6](https://github.com/pbek/nixbit/issues/6))
  - The autostart entry will be re-created on next start once

## 0.2.3

- The memory usage calculation has been improved to show correct values in the
  system resource monitoring bar and is now using `/proc/meminfo` now instead
  of the `free` command
- The page title was changed to `System Configuration`

## 0.2.2

- The color scheme of the application has been improved for better visibility
  in dark and light modes

## 0.2.1

- The issue with the UI of the old version of Nixbit showing up after an update
  has been fixed (for [#11](https://github.com/pbek/nixbit/issues/11))

## 0.2.0

- Some font sizes have been increased for better readability
- The `nixos-rebuild` command call was simplified in `build` mode
- The hostname will now be sanitized in case the user adds unsupported characters
- There now is a new `--debug` CLI argument to use the application with a different
  settings directory and in `build` mode by default
- The window size and position will now be remembered and restored on next start
- There now is a `Build Host` setting in the settings dialog to set the
  hostname to build the configuration on

## 0.1.5

- The _Pause_ button is now hidden in _Switch_ mode, because you can't sleep the
  permission-elevated process (for [#8](https://github.com/pbek/nixbit/issues/8))

## 0.1.4

- The used and available memory will now be shown correctly in the system
  resource monitoring bar (for [#9](https://github.com/pbek/nixbit/issues/9))
- Fix pausing of upgrade process in build mode (for [#8](https://github.com/pbek/nixbit/issues/8))

## 0.1.3

- There now is a checkbox to allow creating or removing an autostart desktop entry
  (for [#6](https://github.com/pbek/nixbit/issues/6))
  - The autostart entry can now be forced to be created with the external config file `/etc/nixbit.conf`
  - There also is a new nix module option `services.nixbit.forceAutostart` for this
- Fix warnings caused by the app icon SVG file (for [#2](https://github.com/pbek/nixbit/issues/2))
- Adapt the window title a bit to show version number at the end
- Wait for network availability after system resume before checking for updates,
  with a 10-second timeout to prevent failures when network is not immediately present
  (for [#7](https://github.com/pbek/nixbit/issues/7))
- A pause/resume button to allow pausing and resuming the running system update
  process was added (for [#8](https://github.com/pbek/nixbit/issues/8))
- Add real-time system resource monitoring (CPU, RAM, network transfer, system load)
  during NixOS updates (for [#9](https://github.com/pbek/nixbit/issues/9))
  - System stats are displayed in a horizontal bar
  - Monitoring automatically starts when an update process begins and stops when it completes
- Some settings were moved to a separate settings dialog to declutter the main window
  (for [#9](https://github.com/pbek/nixbit/issues/9))

## 0.1.2

- When you change the repository URL, the existing repository will be removed
  and re-cloned after a confirmation to avoid issues with mismatched repositories
  (for [#3](https://github.com/pbek/nixbit/issues/3))
- Don't allow the repository URL to be changed, if it was overwritten by the
  external config file (for [#5](https://github.com/pbek/nixbit/issues/5))

## 0.1.1

- The repository url can now be overwritten with an external config file `/etc/nixbit.conf`
  (for [#5](https://github.com/pbek/nixbit/issues/5))
  - This can be done by a nix module or other system configuration management tools
- The repository path will now always be the same and does not depend on the git repository name

## 0.1.0

- Initial release of Nixbit
- NixOS system update functionality
- User interface built with Qt/QML
- Setup of development environment with Nix flakes and devenv
- Terminal output panel for monitoring commands
- Tray icon support
- Automatic fetching of Git repository updates
- Settings for repository URL, hostname, fetch interval and hiding to tray
