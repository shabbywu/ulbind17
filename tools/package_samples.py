"""Package CMake's sample outputs and runtime resources into one platform ZIP."""

import argparse
from pathlib import Path
import shutil
import tempfile
import zipfile


def package(manifest: Path, source: Path, output: Path) -> None:
    entries = [line.split("|", 1) for line in manifest.read_text().splitlines() if line]
    if not entries:
        raise ValueError("Sample manifest is empty")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary) / output.stem
        root.mkdir()
        for name, location in entries:
            item = Path(location)
            if not item.exists():
                raise FileNotFoundError(f"Missing sample output: {item}")
            destination = root / name
            destination.mkdir()
            if item.is_dir():
                # Keep the entire macOS bundle, including dylibs and resources.
                shutil.copytree(item, destination / item.name)
            else:
                shutil.copy2(item, destination / item.name)
                # Runtime libraries sit beside each executable. Ignore build artifacts.
                for sibling in item.parent.iterdir():
                    if sibling.is_file() and (
                        sibling.suffix.lower() in {".dll", ".dylib"}
                        or sibling.name.endswith(".so")
                        or ".so." in sibling.name
                    ):
                        shutil.copy2(sibling, destination / sibling.name)
                assets = item.parent / "assets"
                if assets.is_dir():
                    shutil.copytree(assets, destination / "assets")
        shutil.copy2(source / "LICENSE", root / "LICENSE")
        shutil.copy2(source / "samples/official/UPSTREAM.md", root / "UPSTREAM.md")
        shutil.copytree(source / "samples/official/licenses", root / "licenses")
        (root / "README.txt").write_text(
            "ulbind17 samples\n\n"
            "Extract the entire archive before running samples.\n"
            "Each directory contains one sample and its runtime files.\n"
            "macOS: open the .app bundle, or run Contents/MacOS/<sample> in a terminal.\n"
            "Linux/Windows: run the executable from its own directory.\n"
            "GameUIHostAdapter: use --scene flight-hud --output frame.ppm.\n"
            "Desktop samples require a graphical session.\n"
            "Binaries are unsigned; operating system launch policies still apply.\n"
            "See sdk-licenses, licenses and UPSTREAM.md for resource licensing.\n",
            encoding="utf-8",
        )
        with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED) as archive:
            for file in sorted(root.rglob("*")):
                if file.is_file():
                    archive.write(file, file.relative_to(root.parent))
    print(f"Packaged {len(entries) - 1} samples: {output}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, required=True)
    parser.add_argument(
        "--source", type=Path, default=Path(__file__).resolve().parents[1]
    )
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    package(arguments.manifest, arguments.source, arguments.output)
