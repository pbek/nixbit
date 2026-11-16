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
 * Returns the web URL comparing two commits on the Git forge
 * @param {string} remoteUrl - The Git remote URL
 * @param {string} fromSha - The full hash of the older commit
 * @param {string} toSha - The full hash of the newer commit
 * @returns {string} The compare URL, or an empty string if unsupported
 */
function compareUrl(remoteUrl, fromSha, toSha) {
  var webUrl = repositoryWebUrl(remoteUrl);
  var shaPattern = /^[0-9a-f]{7,40}$/;
  if (
    !webUrl ||
    !shaPattern.test(fromSha || "") ||
    !shaPattern.test(toSha || "")
  ) {
    return "";
  }

  var host = webUrl.split("/")[2].toLowerCase();
  // Bitbucket and SourceHut have no comparable page
  if (host === "bitbucket.org" || host.indexOf("sr.ht") !== -1) {
    return "";
  }
  if (host.indexOf("gitlab") !== -1) {
    return webUrl + "/-/compare/" + fromSha + "..." + toSha;
  }
  // GitHub, Codeberg, Forgejo and Gitea
  return webUrl + "/compare/" + fromSha + "..." + toSha;
}

/**
 * Returns the repository path on the Git forge, e.g. "owner/repo"
 * @param {string} remoteUrl - The Git remote URL
 * @returns {string} The repository path, or an empty string if unknown
 */
function repositoryPath(remoteUrl) {
  var webUrl = repositoryWebUrl(remoteUrl);
  return webUrl ? webUrl.split("/").slice(3).join("/") : "";
}

/**
 * Formats a point in time relative to now, e.g. "3 hours ago"
 * @param {number} timestamp - Milliseconds since the epoch
 * @param {number} now - Milliseconds since the epoch of the current time
 * @returns {string} The relative time
 */
function relativeTime(timestamp, now) {
  var seconds = Math.max(0, Math.floor((now - timestamp) / 1000));
  var units = [
    [31536000, "year"],
    [2592000, "month"],
    [604800, "week"],
    [86400, "day"],
    [3600, "hour"],
    [60, "minute"],
  ];
  for (var i = 0; i < units.length; i++) {
    var value = Math.floor(seconds / units[i][0]);
    if (value >= 1) {
      return value + " " + units[i][1] + (value === 1 ? "" : "s") + " ago";
    }
  }
  return "just now";
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
