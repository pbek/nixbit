#!/usr/bin/env python3
"""Capture the real Nixbit UI with disposable, offline demo backends."""

import argparse
from datetime import datetime
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
TEMP_ROOT = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
SCENES = (
    "mainwindow", "mainwindow-nh", "monitor", "settings", "settings-repository", "settings-hosts",
    "settings-notifications", "settings-performance", "settings-debug", "about",
    "commits", "generations", "logs", "packages", "packages-details",
    "packages-search", "packages-remove", "packages-progress", "packages-unfree",
    "packages-conflict",
)
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def validate_png(path):
    with path.open("rb") as image:
        header = image.read(24)
    if len(header) != 24 or not header.startswith(PNG_SIGNATURE):
        raise RuntimeError(f"Not a PNG image: {path}")
    dimensions = tuple(int.from_bytes(header[i:i + 4], "big") for i in (16, 20))
    if not all(dimension > 0 for dimension in dimensions):
        raise RuntimeError(f"Empty PNG image: {path}")
    return dimensions


def publish(source):
    if source is None:
        captures = sorted((ROOT / "build-screenshots").glob("*/capture.json"),
                          key=lambda path: path.stat().st_mtime_ns)
        if not captures:
            raise RuntimeError("Run `just screenshots` or supply a capture directory first.")
        source = captures[-1].parent
    source = source.resolve()
    images = [source / f"{scene}.png" for scene in SCENES
              if (source / f"{scene}.png").is_file()]
    if not images:
        raise RuntimeError(f"No scene screenshots in {source}")
    for image in images:
        validate_png(image)
    if not shutil.which("cwebp"):
        raise RuntimeError("cwebp is required for publishing (nix develop or nix shell nixpkgs#libwebp).")
    # Convert everything before replacing any reviewed image.
    with tempfile.TemporaryDirectory(prefix="nixbit-publish-", dir=TEMP_ROOT) as temporary:
        for image in images:
            subprocess.run(["cwebp", "-quiet", "-lossless", str(image), "-o",
                            str(Path(temporary) / image.with_suffix(".webp").name)], check=True)
        for image in images:
            target = ROOT / "screenshots" / image.with_suffix(".webp").name
            shutil.copyfile(Path(temporary) / target.name, target)
            print(f"Published {target}")


def desktop_command(env, *arguments):
    return subprocess.check_output(arguments, env=env, text=True, timeout=15).strip()


def verify_window(env, window, process):
    if (process.poll() is not None or
            desktop_command(env, "kdotool", "getactivewindow") != window or
            desktop_command(env, "kdotool", "getwindowpid", window) != str(process.pid)):
        raise RuntimeError("Demo window lost focus or changed ownership; framed image discarded.")


def capture_frame(command, env, ready, image, timeout, log):
    """Ask KWin/Spectacle for the demo's decorations, never the entire desktop."""
    try:
        previous = desktop_command(env, "kdotool", "getactivewindow")
    except subprocess.CalledProcessError:
        previous = ""
    process = subprocess.Popen(command + ["--screenshot-keep-open"], env=env,
                               stdout=log, stderr=log)
    try:
        deadline = time.monotonic() + timeout
        while not ready.is_file():
            if process.poll() is not None:
                raise RuntimeError("The demo exited before it was ready for framed capture.")
            if time.monotonic() >= deadline:
                raise RuntimeError("Timed out waiting for the demo window.")
            time.sleep(0.1)
        _, client_height = validate_png(ready)
        # Plasma may append the application name to the window caption.
        windows = []
        while time.monotonic() < deadline:
            if process.poll() is not None:
                raise RuntimeError("The demo exited before its window could be identified.")
            windows = desktop_command(env, "kdotool", "search", "--all", "--pid", str(process.pid),
                                      "--title", "NixOS Updater").splitlines()
            if len(windows) == 1:
                break
            time.sleep(0.2)
        if len(windows) != 1:
            raise RuntimeError(f"Could not uniquely identify the demo window for PID {process.pid}: {windows}")
        window = windows[0]
        # Leave room around the frame and its shadow, rather than hugging an edge.
        desktop_command(env, "kdotool", "windowmove", window, "100", "60")
        desktop_command(env, "kdotool", "windowactivate", window)
        time.sleep(1)
        verify_window(env, window, process)
        staged = ready.with_name("framed.png")
        subprocess.run(["spectacle", "--new-instance", "--background", "--nonotify", "--activewindow",
                        "--delay", "1000", "--output", str(staged)],
                       env=env, stdout=log, stderr=log, check=True, timeout=timeout)
        verify_window(env, window, process)
        _, framed_height = validate_png(staged)
        if framed_height <= client_height:
            raise RuntimeError("The compositor capture did not include a window frame.")
        shutil.copyfile(staged, image)
    finally:
        if process.poll() is None:
            process.terminate()
            try:
                process.wait(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        if previous:
            try:
                desktop_command(env, "kdotool", "windowactivate", previous)
            except subprocess.SubprocessError:
                pass


def capture(binary, output, scenes, timeout, framed=False):
    if framed:
        for tool in ("kdotool", "spectacle"):
            if not shutil.which(tool):
                raise RuntimeError(f"{tool} is required for --framed (use nix develop or a Plasma desktop).")
        if os.environ.get("KDE_SESSION_VERSION") != "6":
            raise RuntimeError("Framed captures require a running Plasma 6 session; use the VM or omit --framed.")
        print("Capturing window frames. Please avoid switching windows until finished.", flush=True)
    binary = binary.resolve()
    if not binary.is_file():
        raise RuntimeError(f"Binary not found: {binary}; run `just flake-build` or use --binary.")
    output = output.resolve()
    if output.exists():
        raise RuntimeError(f"Output already exists: {output}; choose a fresh directory.")
    output.mkdir(parents=True)
    version = subprocess.check_output([str(binary), "--version"], text=True, timeout=30).strip()
    manifest = {"version": version, "scenes": [], "capture":
                "Plasma window with decorations and shadow" if framed else "Qt window contents (no desktop or decorations)"}
    for scene in scenes:
        image = output / f"{scene}.png"
        with tempfile.TemporaryDirectory(prefix="nixbit-screenshot-", dir=TEMP_ROOT) as temporary:
            base = Path(temporary)
            env = os.environ.copy()
            env["QT_FORCE_STDERR_LOGGING"] = "1"
            # Let each Nix-wrapped Qt tool use its own matching QML modules.
            if framed:
                env.pop("QML_IMPORT_PATH", None)
            # Appearance only: never copy the user's Nixbit settings or profile.
            desktop_config = Path(env.get("XDG_CONFIG_HOME", Path.home() / ".config"))
            desktop_data = env.get("XDG_DATA_HOME", str(Path.home() / ".local/share"))
            env["XDG_DATA_DIRS"] = desktop_data + ":" + env.get("XDG_DATA_DIRS", "/usr/local/share:/usr/share")
            config = base / "config"
            config.mkdir()
            for name in ("kdeglobals", "breezerc", "Kvantum", "qt6ct"):
                source = desktop_config / name
                if source.is_dir():
                    shutil.copytree(source, config / name)
                elif source.is_file():
                    shutil.copyfile(source, config / name)
            for variable, directory in (("HOME", base), ("XDG_CONFIG_HOME", config),
                                        ("XDG_DATA_HOME", base / "data"),
                                        ("XDG_CACHE_HOME", base / "cache"),
                                        ("XDG_STATE_HOME", base / "state")):
                directory.mkdir(exist_ok=True)
                env[variable] = str(directory)
            with (output / f"{scene}.log").open("w+") as log:
                try:
                    ready = base / "ready.png" if framed else image
                    command = [str(binary), "--screenshot-scene", scene, "--screenshot-output", str(ready)]
                    if framed:
                        capture_frame(command, env, ready, image, timeout, log)
                    else:
                        subprocess.run(command, env=env, stdout=log, stderr=log, check=True, timeout=timeout)
                except (RuntimeError, subprocess.SubprocessError):
                    log.flush()
                    log.seek(0)
                    print(log.read()[-6000:], file=sys.stderr)
                    raise
        validate_png(image)
        manifest["scenes"].append(scene)
        print(image, flush=True)
    (output / "capture.json").write_text(json.dumps(manifest, indent=2) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, default=ROOT / "result/bin/nixbit")
    parser.add_argument("--output", type=Path, default=ROOT / "build-screenshots" /
                        datetime.now().strftime("%Y%m%d-%H%M%S-%f"))
    parser.add_argument("--scenes", "--presets", nargs="+", choices=SCENES, default=SCENES)
    parser.add_argument("--overview", action="store_true", help="Capture every scene (the default)")
    parser.add_argument("--publish", type=Path, nargs="?", const=False,
                        help="Publish reviewed images, defaulting to the latest capture")
    parser.add_argument("--timeout", type=float, default=45)
    parser.add_argument("--framed", action="store_true", help="Include Plasma window borders and shadows (requires kdotool and Spectacle)")
    args = parser.parse_args()
    try:
        if args.publish is not None:
            publish(None if args.publish is False else args.publish)
        else:
            if args.timeout <= 0:
                parser.error("--timeout must be positive")
            capture(args.binary, args.output, SCENES if args.overview else args.scenes, args.timeout, args.framed)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f"{error}\nCapture logs are retained in {args.output}.\n")


if __name__ == "__main__":
    main()
