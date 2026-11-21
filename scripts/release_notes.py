"""Extract reviewed release notes and reject version drift before packaging."""
import pathlib
import re
import sys


def release_notes(root: pathlib.Path, tag: str) -> str:
    if not re.fullmatch(r"v\d+\.\d+\.\d+", tag):
        raise ValueError("Expected a stable semantic-version tag")
    version = tag[1:]
    cmake = (root / "CMakeLists.txt").read_text()
    pubspec = (root / "companion/desktop/pubspec.yaml").read_text()
    if f'set(PROJECT_VER "{version}")' not in cmake or not re.search(
        rf"^version: {re.escape(version)}\+\d+$", pubspec, re.M
    ):
        raise ValueError("Tag, firmware and companion versions must agree")
    sections = re.split(r"(?m)^## ", (root / "CHANGELOG.md").read_text())
    for section in sections[1:]:
        heading, _, body = section.partition("\n")
        if re.fullmatch(rf"{re.escape(version)} - \d{{4}}-\d{{2}}-\d{{2}}", heading):
            if not body.strip():
                raise ValueError("Release notes cannot be empty")
            return f"# WristFoundry {version}\n\n{body.strip()}\n"
    raise ValueError("Finalize a dated changelog section before tagging")


if __name__ == "__main__":
    try:
        print(release_notes(pathlib.Path(__file__).resolve().parents[1], sys.argv[1]))
    except (IndexError, ValueError) as error:
        raise SystemExit(str(error)) from error
