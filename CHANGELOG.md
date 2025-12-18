# Nixbit Changelog

## 0.6.2

- **Fixed "Ignore commits" button triggering unwanted rebuild/switch**
  - The "Ignore commits" button in the Commits Dialog now only pulls from git without triggering a build or switch operation
  - Previously, clicking this button would automatically trigger the currently selected rebuild mode (build or switch)
  - Now the button only downloads git changes as intended, with changes applied only in the next manual build

## 0.6.1

- **Fixed search functionality scrolling issues**
  - Search now properly wraps around from bottom to top when reaching the last match
  - Fixed terminal staying locked to bottom when searching while at the bottom of output
  - Search navigation now disables auto-scroll to prevent conflicting scroll behavior
  - Terminal no longer flickers or jumps to bottom when searching during active build output
  - Search results now consistently scroll to center the match in the viewport
- **Added "Ignore commits" button to Commits Dialog**
  - New button pulls git changes to remove them from the pending commits list
  - Button is only enabled when there are commits behind and git is not busy
  - Tooltip clarifies that it downloads commits rather than ignoring them
  - Changes will be applied in the next build operation
- **Added About dialog**
  - New "About" menu item in the Tools menu
  - Displays application version, description, and links
  - Includes direct links to GitHub repository and issue tracker
  - Shows license and author information

## 0.6.0

- **Added search functionality for terminal output**
  - Press `Ctrl+F` to open search bar and find text in terminal output
  - Navigate between matches with `F3` (next) and `Shift+F3` (previous)
  - Search bar shows current match position (e.g., "3/10")
  - Use arrow keys (Up/Down) or Enter/Shift+Enter to navigate matches
  - Press `Escape` to close the search bar
  - Search automatically highlights matches and scrolls to them
  - Case-insensitive search works with both plain text and syntax-highlighted output
- **Improved terminal output scrolling behavior**
  - Terminal output no longer forces auto-scroll when users scroll up to read earlier logs
  - Added "Scroll to Bottom" button that appears when scrolled away from the bottom
  - Auto-scroll intelligently resumes only when user is near the bottom (within 50 pixels)
  - Allows reviewing earlier build output while a process is still running without interruption

## 0.5.5

- **Fixed generation refresh after "Build & Switch"**
  - Generations list now properly refreshes after completing a "Build & Switch" operation
  - Fixed issue where the generation check was using combo box value instead of actual operation mode
  - Ensures the generations dialog shows the newly created generation immediately
- **Enhanced debug mode visibility**
  - Tray icons now use different colors in debug mode for easier identification
    - Up-to-date icon: cyan/blue instead of green
    - Updates available icon: darker orange instead of bright orange
    - Unknown status icon: purple/magenta instead of gray/blue
  - Tooltip now indicates "(Debug Mode)" when running with `--debug` flag

## 0.5.4

- **Fixed log file cleanup bug**
  - Log files are now properly retained instead of being immediately deleted after creation
  - The cleanup function was incorrectly deleting the newest logs instead of the oldest ones
  - Now correctly maintains the configured number of log files (default: 10)
- **Fixed exit code handling when password prompt is cancelled**
  - When cancelling the password prompt during a "switch" operation, the process now correctly reports a non-zero exit code
  - Previously, the cleanup command would mask the failure, making cancelled operations appear successful (exit code 0)
  - Now properly preserves the pkexec exit code while still cleaning up temporary files

## 0.5.3

- **Fixed "Build & Switch" to update git repository first**
  - "Build & Switch" now pulls from the git repository before building, just like "Build System" and "Update System" do
  - Ensures all operations work with the latest code from the repository
  - Maintains consistency across all update operations

## 0.5.2

- **Fixed build success message display**
  - Build logs now show the complete "Done. The new configuration is..." message including the full Nix store path
  - Previously the path was being cut off after "Done. The new configuration is "

## 0.5.1

- **Enhanced build logs display**
  - Logs now show whether the build was done using "build" or "switch" mode
  - Added build type badge (BUILD/SWITCH) in the logs dialog for quick identification
  - Display file size for each log (formatted in B, KB, MB, or GB)
  - Log filenames now include build type for better organization
  - Legacy log files (without build type) are still supported and default to "build" mode
- **Added keyboard shortcuts for terminal output**
  - `Ctrl+C` to copy selected text
  - `Ctrl+A` to select all terminal output
  - Shortcuts are displayed in the context menu

## 0.5.0

- **Fixed critical memory issue** - Application could consume a lot of RAM and
  become unresponsive during long builds
- **Added output buffer limiting** - Terminal output now limited to last 5,000
  lines by default (configurable 500-20,000)
  - Prevents unbounded memory growth during lengthy builds
  - Older output automatically truncated with indicator
- **Rewrote syntax highlighting in C++** - Moved from JavaScript to native C++ implementation
  - Uses `QRegularExpression` with optimized and pre-compiled patterns
  - Significantly faster and more memory-efficient than JavaScript regex
  - Caching prevents re-processing of unchanged output
- **Added build log storage and management**
  - Automatically saves last 10 build logs to disk (configurable 0-100)
  - New "View Logs" button to access past build logs
  - Logs dialog shows timestamp, exit status, and quick actions
  - Open logs in system default editor with one click
  - Delete individual logs with confirmation
  - Logs stored in `~/.local/share/nixbit/logs/` directory
- **Redesigned Settings Dialog** - KDE System Settings-style interface
  - Vertical category menu with icons (General, Performance, Repository, Build Hosts)
  - Split-view layout for better organization and navigation
  - Larger dialog (900x600) for improved usability
  - Each category has its own dedicated page with clear section headers
- **Repository quick access buttons** - Added to main window
  - "Open in file manager" button opens repository folder with xdg-open
  - "Open terminal" button launches terminal in repository directory
  - Both buttons positioned next to Repository URL field for easy access
  - Support for multiple terminal emulators (konsole, gnome-terminal, xfce4-terminal, alacritty, kitty, ghostty, xterm)
- **New settings in Settings Dialog**
  - Max Terminal Lines: Configure memory usage (500-20,000 lines, default 5,000)
  - Max Stored Logs: Configure log retention (0-100 files, default 10, 0 = unlimited)
- **Added "Build & Switch" button** - New button to chain build and switch operations
  - Runs `nixos-rebuild build` first using the build host setting
  - If build succeeds, automatically runs `nixos-rebuild switch` using the switch host setting
  - If build fails, switch phase is cancelled with clear error message
  - Shows progress messages for each phase
  - Allows testing configuration with a remote build host before applying locally
- **Dynamic button labels** - Main action button now reflects the selected mode
  - Shows "Build System" when build mode is selected
  - Shows "Switch System" when switch mode is selected (was "Update System")
  - Provides clearer indication of what action will be performed

## 0.4.0

- Added detailed commit information display for available updates
  - New "View Commits" button appears when the repository is behind remote
  - Opens a dialog showing all commits available for update
  - Each commit displays:
    - Short SHA hash (7 characters, monospace font)
    - Commit author name
    - Commit message
    - Relative time (e.g., "2 hours ago", "3 days ago")
    - Full timestamp
  - Dialog includes a refresh button to check for new updates
  - Commits are sorted by time with most recent first
  - Commit hashes are clickable links when using a GitHub repository
    - Automatically detects GitHub repositories (git@github.com or https://github.com)
    - Clicking a commit hash opens the commit page in your web browser
    - Handles both SSH (git@github.com:user/repo.git) and HTTPS formats
    - Non-GitHub repositories show plain text commit hashes
- Console output now supports text selection with mouse and keyboard
  - Right-click context menu added with Copy, Select All, and Deselect options
  - Standard keyboard shortcuts (Ctrl+C) work for copying selected text
- The highlighting colors in the generations list dialog have been improved for better visibility

## 0.3.2

- The tray icon will now correctly reflect the amount of commits behind

## 0.3.1

- Moved Local Path display from main window to Settings Dialog
- Added automatic data refresh when window becomes visible
  - Checks for repository updates when window is shown or unhidden
  - Refreshes NixOS generations list when window becomes visible
- Added build result status message that shows success or failure after builds
  - Displays a green success message ("✓ Build completed successfully!") when exit code is 0
  - Displays a red error message with exit code ("✗ Build failed with exit code N") when build fails
  - Message persists until another action is taken (new build, clear output, etc.)
  - Users can manually dismiss the message using the close button
  - Correctly interprets Nix build process exit codes
- Added syntax highlighting to terminal output for better readability
  - Success messages (e.g., "Done. The new configuration is", "Process finished with exit code: 0") highlighted in green
  - Error messages (e.g., "error:", "failed", "Process finished with exit code: [1-9]") highlighted in red and bold
  - Warning messages highlighted in yellow/orange
  - Build activity messages (e.g., "building", "copying", "evaluating") highlighted in cyan
  - Process status markers (e.g., "=== Process finished ===") highlighted in magenta and bold

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
