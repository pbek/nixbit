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

ROOT = Path(__file__).resolve().parent.parent
TEMP_ROOT = "/tmp/opencode" if Path("/tmp/opencode").is_dir() else None
SCENES = (
    "mainwindow", "monitor", "settings", "settings-repository", "settings-hosts",
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
    if not all(int.from_bytes(header[i:i + 4], "big") > 0 for i in (16, 20)):
        raise RuntimeError(f"Empty PNG image: {path}")


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


def capture(binary, output, scenes, timeout):
    binary = binary.resolve()
    if not binary.is_file():
        raise RuntimeError(f"Binary not found: {binary}; run `just flake-build` or use --binary.")
    output = output.resolve()
    if output.exists():
        raise RuntimeError(f"Output already exists: {output}; choose a fresh directory.")
    output.mkdir(parents=True)
    version = subprocess.check_output([str(binary), "--version"], text=True, timeout=30).strip()
    manifest = {"version": version, "scenes": [], "capture": "Qt window contents (no desktop or decorations)"}
    for scene in scenes:
        image = output / f"{scene}.png"
        with tempfile.TemporaryDirectory(prefix="nixbit-screenshot-", dir=TEMP_ROOT) as temporary:
            base = Path(temporary)
            env = os.environ.copy()
            env["QT_FORCE_STDERR_LOGGING"] = "1"
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
                    subprocess.run([str(binary), "--screenshot-scene", scene,
                                    "--screenshot-output", str(image)], env=env,
                                   stdout=log, stderr=log, check=True, timeout=timeout)
                except subprocess.SubprocessError:
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
    args = parser.parse_args()
    try:
        if args.publish is not None:
            publish(None if args.publish is False else args.publish)
        else:
            if args.timeout <= 0:
                parser.error("--timeout must be positive")
            capture(args.binary, args.output, SCENES if args.overview else args.scenes, args.timeout)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        parser.exit(1, f"{error}\nCapture logs are retained in {args.output}.\n")


if __name__ == "__main__":
    main()
