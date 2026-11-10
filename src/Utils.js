// Utility functions for Nixbit

/**
 * Converts a Git remote URL to the web URL of the repository
 * Supports https://, ssh:// and scp-like (git@host:owner/repo) URLs
 * @param {string} remoteUrl - The Git remote URL
 * @returns {string} The web URL, or an empty string for local or unknown URLs
 */
function repositoryWebUrl(remoteUrl) {
  var url = (remoteUrl || "").trim();
  var host = "";
  var path = "";

  var scpLike = url.match(/^(?:[^@/]+@)?([^:/]+):(?!\/)(.+)$/);
  var withScheme = url.match(
    /^(?:https?|ssh|git):\/\/(?:[^@/]+@)?([^:/]+)(?::\d+)?\/(.+)$/,
  );
  if (withScheme) {
    host = withScheme[1];
    path = withScheme[2];
  } else if (scpLike) {
    host = scpLike[1];
    path = scpLike[2];
  } else {
    return "";
  }

  path = path.replace(/\/+$/, "").replace(/\.git$/, "");
  if (!host || !path) {
    return "";
  }
  return "https://" + host + "/" + path;
}

/**
 * Returns the web URL of a commit on the Git forge hosting the repository
 * @param {string} remoteUrl - The Git remote URL
 * @param {string} sha - The full commit hash
 * @returns {string} The commit URL, or an empty string if unknown
 */
function commitUrl(remoteUrl, sha) {
  var webUrl = repositoryWebUrl(remoteUrl);
  if (!webUrl || !/^[0-9a-f]{7,40}$/.test(sha || "")) {
    return "";
  }

  var host = webUrl.split("/")[2].toLowerCase();
  if (host === "bitbucket.org") {
    return webUrl + "/commits/" + sha;
  }
  if (host.indexOf("gitlab") !== -1) {
    return webUrl + "/-/commit/" + sha;
  }
  // GitHub, Codeberg, Forgejo, Gitea and SourceHut
  return webUrl + "/commit/" + sha;
}

/**
 * Returns the host name of the Git forge, e.g. "github.com"
 * @param {string} remoteUrl - The Git remote URL
 * @returns {string} The host name, or an empty string if unknown
 */
function forgeName(remoteUrl) {
  var webUrl = repositoryWebUrl(remoteUrl);
  return webUrl ? webUrl.split("/")[2] : "";
}

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
