"""Unit tests for capture isolation, scene coverage and reviewed publishing."""

import importlib.util
import json
import os
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

SPEC = importlib.util.spec_from_file_location("screenshots", Path(__file__).with_name("screenshots.py"))
screenshots = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(screenshots)


class ScreenshotTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            dir="/tmp/opencode" if Path("/tmp/opencode").is_dir() else None)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def png(self, path):
        path.write_bytes(screenshots.PNG_SIGNATURE + b"\0\0\0\rIHDR" +
                         (1100).to_bytes(4, "big") + (900).to_bytes(4, "big"))

    def test_scene_lists_match_entry_point(self):
        source = (screenshots.ROOT / "src/main.cpp").read_text()
        scene_list = source.split("const QStringList scenes =", 1)[1].split(";", 1)[0]
        self.assertEqual(set(screenshots.SCENES), set(re.findall(r'"([^"]+)"', scene_list)))

    def test_validate_rejects_invalid_image(self):
        image = self.root / "broken.png"
        image.write_bytes(b"not an image")
        with self.assertRaises(RuntimeError):
            screenshots.validate_png(image)
        self.png(image)
        screenshots.validate_png(image)

    def test_capture_isolates_profile_and_retains_manifest(self):
        binary = self.root / "nixbit"
        binary.touch()
        config = self.root / "real-config"
        config.mkdir()
        (config / "kdeglobals").write_text("appearance")
        (config / "nixbit.conf").write_text("private settings")
        output = self.root / "capture"

        def run(command, **kwargs):
            env = kwargs["env"]
            self.assertNotEqual(env["HOME"], os.environ.get("HOME"))
            isolated = Path(env["XDG_CONFIG_HOME"])
            self.assertEqual((isolated / "kdeglobals").read_text(), "appearance")
            self.assertFalse((isolated / "nixbit.conf").exists())
            for variable in ("XDG_DATA_HOME", "XDG_CACHE_HOME", "XDG_STATE_HOME"):
                self.assertTrue(Path(env[variable]).is_dir())
            self.assertEqual(command[1:3], ["--screenshot-scene", "packages"])
            self.png(Path(command[-1]))

        with patch.dict(os.environ, {"XDG_CONFIG_HOME": str(config)}), \
                patch.object(screenshots.subprocess, "check_output", return_value="nixbit 1.6.0\n"), \
                patch.object(screenshots.subprocess, "run", side_effect=run):
            screenshots.capture(binary, output, ["packages"], 45)
        self.assertEqual(json.loads((output / "capture.json").read_text())["scenes"], ["packages"])
        with self.assertRaises(RuntimeError):
            screenshots.capture(binary, output, ["packages"], 45)

    def test_publish_validates_every_image_before_replacing(self):
        source = self.root / "capture"
        source.mkdir()
        self.png(source / "mainwindow.png")
        (source / "packages.png").write_bytes(b"invalid")
        with patch.object(screenshots.subprocess, "run") as run:
            with self.assertRaises(RuntimeError):
                screenshots.publish(source)
            run.assert_not_called()

    def test_publish_keeps_uncaptured_scenes(self):
        source = self.root / "capture"
        source.mkdir()
        self.png(source / "packages.png")
        gallery = self.root / "screenshots"
        gallery.mkdir()
        (gallery / "mainwindow.webp").write_bytes(b"existing")

        def convert(command, **kwargs):
            Path(command[-1]).write_bytes(b"converted WebP")

        with patch.object(screenshots, "ROOT", self.root), \
                patch.object(screenshots.shutil, "which", return_value="cwebp"), \
                patch.object(screenshots.subprocess, "run", side_effect=convert):
            screenshots.publish(source)
        self.assertEqual((gallery / "packages.webp").read_bytes(), b"converted WebP")
        self.assertEqual((gallery / "mainwindow.webp").read_bytes(), b"existing")

    def test_failed_capture_does_not_mark_run_complete(self):
        binary = self.root / "nixbit"
        binary.touch()
        output = self.root / "failed"
        with patch.object(screenshots.subprocess, "check_output", return_value="nixbit 1.6.0"), \
                patch.object(screenshots.subprocess, "run", side_effect=TimeoutError("capture timed out")):
            with self.assertRaises(TimeoutError):
                screenshots.capture(binary, output, ["packages"], 1)
        self.assertFalse((output / "capture.json").exists())
        self.assertTrue((output / "packages.log").is_file())


if __name__ == "__main__":
    unittest.main()
