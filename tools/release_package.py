#!/usr/bin/env python3
"""Create a clean source ZIP for a ChessZero release.

Transient build directories, Python caches, editor metadata, and VCS metadata are
excluded. The script intentionally packages source, tests, tools, networks, docs,
and CMake metadata so the result is reproducible from a fresh checkout/extract.
"""
from __future__ import annotations

import argparse
import pathlib
import zipfile

EXCLUDED_DIRS = {"build", "build-android", ".git", "__pycache__", ".pytest_cache"}
EXCLUDED_SUFFIXES = {".pyc", ".o", ".obj"}


def include(path: pathlib.Path) -> bool:
    return (
        not any(part in EXCLUDED_DIRS or part.startswith("build_") or part.startswith("cmake-build") for part in path.parts)
        and path.suffix not in EXCLUDED_SUFFIXES
    )


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("root", type=pathlib.Path)
    ap.add_argument("output", type=pathlib.Path)
    args = ap.parse_args()

    root = args.root.resolve()
    output = args.output.resolve()
    if output.exists():
        output.unlink()

    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=9) as zf:
        for path in sorted(root.rglob("*")):
            if path.is_file() and include(path):
                arc = pathlib.Path(root.name) / path.relative_to(root)
                zf.write(path, arc.as_posix())
    print(f"wrote {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
