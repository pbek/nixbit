# Automatic screenshots

Nixbit captures its real QML windows and dialogs using in-memory demo backends.
No repository is cloned, no Nix command runs, no user package is changed, and
no autostart entry is created. Example package versions and generations are
illustrative, not a live report of nixpkgs.

## Current desktop

```bash
just flake-build
just screenshots
just screenshots --framed
just screenshots --scenes packages packages-details packages-search
```

Use `--binary build/nixbit` for a local CMake build. Captures are written to a
new timestamped directory under `build-screenshots/`, with a log for each scene
and `capture.json` identifying the binary version and completed scenes. Use
`--output <new-directory>` to choose the destination.

Only desktop appearance settings are copied to disposable directories. Qt
renders the application window contents directly, including embedded dialogs;
desktop contents, other windows, window decorations and the pointer are never
captured in the default frameless mode. This works on X11 and Wayland without
Spectacle or focus automation.
For CI without a desktop, use `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software`.
Prefer the VM for consistent Plasma styling and matching Qt plugins.

Use `--framed` on a Plasma 6 desktop to include the real window borders, title
bar and shadow. This requires `kdotool` and Spectacle, provided by `nix develop`
and devenv. The application stays open until Spectacle captures its active
window; the script checks window ownership and focus before and after capture,
discards images if focus changed, and restores the previously active window.
Avoid switching windows during framed capture. No pointer or full-desktop
capture is requested. Spectacle also uses disposable settings.

## Isolated NixOS Plasma VM

```bash
just screenshots-vm
just screenshots-vm-interactive
```

Requires Linux, hardware virtualization, read/write access to `/dev/kvm`, and
a Nix builder configured to expose KVM (`system-features = kvm`). The VM uses
the locked nixpkgs and a disposable Plasma session. It needs about 4 GiB of RAM.
Nix may download substantial dependencies on the first run.

The VM recipe captures every scene **with borders and shadows**, copies results into a fresh
`build-screenshots/<timestamp>-vm` directory, and does not replace `result`.
Extra arguments go to `nix build`, for example `--show-trace`.
Each recipe run changes the capture derivation so existing screenshots are not
silently reused from the Nix cache. Application dependencies remain cached.

In the interactive Python driver, run `start_all()` to boot or `test_script()`
to execute the capture workflow. The underlying check is
`checks.<system>.screenshots` (also available on aarch64-linux builders).

## Review and publish

```bash
just screenshots-publish build-screenshots/<capture-directory>
```

With no directory argument, the latest completed capture is selected.
Publishing requires `cwebp` (provided by `nix develop` and devenv). Images are
converted to lossless WebP before replacing files in `screenshots/`. Partial
captures replace only their scenes; existing images of other scenes are kept.
The main README retains its `screenshots/mainwindow.webp` link.

## Scene coverage

| Scenes                                                             | Interface                                                                              |
| ------------------------------------------------------------------ | -------------------------------------------------------------------------------------- |
| `mainwindow`, `monitor`                                            | Updates, running revision, generations, user packages, build output and system monitor |
| `settings`                                                         | General settings                                                                       |
| `settings-repository`, `settings-hosts`                            | Repository and remote build hosts                                                      |
| `settings-notifications`, `settings-performance`, `settings-debug` | Remaining settings pages                                                               |
| `about`, `commits`, `generations`, `logs`                          | About, available updates, generation history and build logs                            |
| `packages`, `packages-details`                                     | Installed user packages, available versions, metadata and expanded details             |
| `packages-search`, `packages-remove`                               | nixpkgs search, system-installed markers and removal confirmation                      |
| `packages-progress`                                                | Package update progress and command output                                             |
| `packages-unfree`, `packages-conflict`                             | Retry prompts for unfree licenses and conflicting files                                |

`--overview` explicitly selects all scenes (also the default); `--presets` is an
alias for `--scenes` for familiarity with the QOwnNotes workflow.

Add future scenes to `scripts/screenshots.py`, the validated scene list in
`src/main.cpp`, and the corresponding QML scene setup. Keep fixtures in
`src/ScreenshotData.qml` aligned with the properties consumed by the real UI.

Run the capture/publishing safety tests with:

```bash
python3 -m unittest discover -s scripts -p 'test_*.py' -v
```
