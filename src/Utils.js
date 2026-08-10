.pragma library

// Utility functions for Nixbit

/**
 * Opens a terminal in the specified directory
 * Tries multiple terminal emulators in order of preference
 * @param {string} path - The directory path to open the terminal in
 * @param {object} processManager - The ProcessManager instance
 */
function openTerminalInDirectory(path, processManager) {
  if (!processManager || !path) {
    return;
  }

  var cmd =
    "if command -v konsole >/dev/null 2>&1; then konsole --workdir '" +
    path +
    "' & " +
    "elif command -v gnome-terminal >/dev/null 2>&1; then gnome-terminal --working-directory='" +
    path +
    "' & " +
    "elif command -v xfce4-terminal >/dev/null 2>&1; then xfce4-terminal --working-directory='" +
    path +
    "' & " +
    "elif command -v alacritty >/dev/null 2>&1; then alacritty --working-directory '" +
    path +
    "' & " +
    "elif command -v kitty >/dev/null 2>&1; then kitty --directory '" +
    path +
    "' & " +
    "elif command -v ghostty >/dev/null 2>&1; then ghostty --working-directory='" +
    path +
    "' & " +
    "elif command -v xterm >/dev/null 2>&1; then cd '" +
    path +
    "' && xterm & " +
    "else notify-send 'Nixbit' 'No supported terminal emulator found'; fi";

  processManager.startDetached("bash", ["-c", cmd]);
}
