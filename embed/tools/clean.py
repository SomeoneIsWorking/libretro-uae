#!/usr/bin/env python3
"""Remove only generated artifacts created by embed verification."""

from __future__ import annotations

import shutil
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
BUILD = ROOT / "build"
BYTECODE = ROOT / "embed" / "tools" / "__pycache__"


def main() -> int:
    targets = (BUILD.resolve(), BYTECODE.resolve())
    expected = (
        ROOT.resolve() / "build",
        ROOT.resolve() / "embed" / "tools" / "__pycache__",
    )
    if targets != expected:
        raise RuntimeError(f"refusing unexpected cleanup targets: {targets}")
    for target in targets:
        if target.is_dir():
            shutil.rmtree(target)
            print(f"removed {target}")
        else:
            print(f"generated path is already absent: {target}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
