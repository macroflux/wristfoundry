import importlib.util
import pathlib
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[1] / "scripts/release_notes.py"
SPEC = importlib.util.spec_from_file_location("release_notes", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class ReleaseNotes(unittest.TestCase):
    def test_requires_matching_versions_and_finalized_notes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = pathlib.Path(directory)
            (root / "companion/desktop").mkdir(parents=True)
            (root / "CMakeLists.txt").write_text('set(PROJECT_VER "0.2.0")')
            (root / "companion/desktop/pubspec.yaml").write_text('version: 0.2.0+2\n')
            (root / "CHANGELOG.md").write_text('## Unreleased: 0.2.0 candidate\n- Pending\n')
            with self.assertRaises(ValueError):
                MODULE.release_notes(root, "v0.2.0")
            (root / "CHANGELOG.md").write_text('## 0.2.0 - 2026-11-21\n- Settings\n\n## 0.1.0 - 2026-11-20\n- Old\n')
            result = MODULE.release_notes(root, "v0.2.0")
            self.assertIn("Settings", result)
            self.assertNotIn("Old", result)
            with self.assertRaises(ValueError):
                MODULE.release_notes(root, "v0.3.0")
            with self.assertRaises(ValueError):
                MODULE.release_notes(root, "main")


if __name__ == "__main__":
    unittest.main()
