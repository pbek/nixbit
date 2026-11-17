"""Unit tests for capture isolation, scene coverage and reviewed publishing."""

import importlib.util
import json
import os
import re
from pathlib import Path
import tempfile
import unittest
from unittest.mock import Mock, patch

SPEC = importlib.util.spec_from_file_location("screenshots", Path(__file__).with_name("screenshots.py"))
screenshots = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(screenshots)


class ScreenshotTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(
            dir="/tmp/opencode" if Path("/tmp/opencode").is_dir() else None)
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)

    def png(self, path, height=900):
        path.write_bytes(screenshots.PNG_SIGNATURE + b"\0\0\0\rIHDR" +
                         (1100).to_bytes(4, "big") + height.to_bytes(4, "big"))

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

    def framed_capture(self, change_focus=False, frame_height=960):
        ready = self.root / "ready.png"
        image = self.root / "published.png"
        self.png(ready)
        process = Mock(pid=1234)
        process.poll.return_value = None
        active = ["original-window"]

        def desktop(env, *arguments):
            action = arguments[1]
            if action == "getactivewindow":
                return active[0]
            if action == "search":
                return "demo-window"
            if action == "getwindowpid":
                return "1234"
            if action == "windowactivate":
                active[0] = arguments[2]
            return ""

        def spectacle(command, **kwargs):
            self.assertIn("--activewindow", command)
            self.assertIn("--background", command)
            self.assertNotIn("--fullscreen", command)
            self.assertNotIn("--no-decoration", command)
            self.assertNotIn("--no-shadow", command)
            self.assertNotIn("--pointer", command)
            self.png(Path(command[-1]), height=frame_height)
            if change_focus:
                active[0] = "unrelated-window"

        with patch.object(screenshots.subprocess, "Popen", return_value=process) as launch, \
                patch.object(screenshots, "desktop_command", side_effect=desktop), \
                patch.object(screenshots.subprocess, "run", side_effect=spectacle), \
                patch.object(screenshots.time, "sleep"):
            if change_focus or frame_height <= 900:
                with self.assertRaisesRegex(RuntimeError, "discarded" if change_focus else "window frame"):
                    screenshots.capture_frame(["nixbit"], {}, ready, image, 45, Mock())
                self.assertFalse(image.exists())
            else:
                screenshots.capture_frame(["nixbit"], {}, ready, image, 45, Mock())
                screenshots.validate_png(image)
            self.assertIn("--screenshot-keep-open", launch.call_args.args[0])
        process.terminate.assert_called_once()
        process.wait.assert_called_once()
        self.assertEqual(active[0], "original-window")

    def test_frame_includes_decorations_and_restores_focus(self):
        self.framed_capture()

    def test_frame_discards_image_if_focus_changes(self):
        self.framed_capture(change_focus=True)

    def test_frame_rejects_undecorated_capture(self):
        self.framed_capture(frame_height=900)

    def test_frame_rejects_foreign_window(self):
        process = Mock(pid=1234)
        process.poll.return_value = None
        with patch.object(screenshots, "desktop_command", side_effect=["demo-window", "5678"]):
            with self.assertRaisesRegex(RuntimeError, "ownership"):
                screenshots.verify_window({}, "demo-window", process)

    def test_frame_requires_capture_tools_before_creating_output(self):
        output = self.root / "not-created"
        with patch.object(screenshots.shutil, "which", return_value=None):
            with self.assertRaisesRegex(RuntimeError, "kdotool"):
                screenshots.capture(self.root / "nixbit", output, ["packages"], 45, framed=True)
        self.assertFalse(output.exists())


if __name__ == "__main__":
    unittest.main()
