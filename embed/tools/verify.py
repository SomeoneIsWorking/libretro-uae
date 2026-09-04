#!/usr/bin/env python3
"""Standalone, asset-free verification for the maintained 68000 embed target."""

from __future__ import annotations

import os
import platform
import re
import shlex
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
EMBED = ROOT / "embed"
SOURCES = (
    ROOT / "sources" / "src" / "m68k_embed.c",
    ROOT / "sources" / "src" / "include" / "uae" / "m68k_embed.h",
    EMBED / "tests" / "m68k_embed_test.c",
)
TRANSLATION_UNITS = (SOURCES[0], SOURCES[2])
CPU_HANDLER = re.compile(r"^op_[0-9a-f]{4}_12_ff$")
FORBIDDEN_SYMBOLS = {"retro_init", "retro_load_game", "retro_run", "memory_init"}
ACTION_REFERENCE = re.compile(r"^\s*-?\s*uses:\s*[^\s@]+@([^\s]+)\s*$", re.MULTILINE)
FULL_COMMIT = re.compile(r"[0-9a-f]{40}")


@dataclass(frozen=True)
class Profile:
    name: str
    c_compiler: str
    compiler_id: str
    executable_suffix: str = ""


def run(command: list[str], *, capture: bool = False) -> subprocess.CompletedProcess[str]:
    print(f"$ {shlex.join(command)}", flush=True)
    return subprocess.run(
        command, cwd=ROOT, check=True, capture_output=capture, text=True
    )


def profile() -> Profile:
    system = platform.system()
    machine = platform.machine().lower()
    if system == "Linux" and machine in {"x86_64", "amd64"}:
        return Profile("linux-x86_64", "clang", "Clang")
    if system == "Darwin" and machine == "arm64":
        return Profile("macos-arm64", "clang", "AppleClang")
    if system == "Windows" and machine in {"x86_64", "amd64"}:
        return Profile("windows-x86_64", "clang-cl", "Clang", ".exe")
    raise RuntimeError(f"the embed CI matrix does not own {system} {machine}")


def llvm_tool(name: str) -> str:
    if located := shutil.which(name):
        return located
    if platform.system() == "Darwin" and (brew := shutil.which("brew")):
        prefix = run([brew, "--prefix", "llvm"], capture=True).stdout.strip()
        candidate = Path(prefix) / "bin" / name
        if candidate.is_file():
            return str(candidate)
    if platform.system() == "Windows" and (program_files := os.environ.get("ProgramFiles")):
        candidate = Path(program_files) / "LLVM" / "bin" / f"{name}.exe"
        if candidate.is_file():
            return str(candidate)
    raise RuntimeError(f"required LLVM tool is unavailable: {name}")


def assert_compiler(build_dir: Path, expected: str) -> None:
    matches = list((build_dir / "CMakeFiles").glob("*/CMakeCCompiler.cmake"))
    if len(matches) != 1:
        raise RuntimeError(f"expected one CMake compiler record, found {len(matches)}")
    if f'set(CMAKE_C_COMPILER_ID "{expected}")' not in matches[0].read_text(encoding="utf-8"):
        raise RuntimeError(f"standalone embed target did not configure with {expected}")


def verify_workflow() -> None:
    workflow = ROOT / ".github" / "workflows" / "embed-ci.yml"
    text = workflow.read_text(encoding="utf-8")
    mutable = [
        reference
        for reference in ACTION_REFERENCE.findall(text)
        if not FULL_COMMIT.fullmatch(reference)
    ]
    required = (
        "ubuntu-24.04",
        "windows-2025",
        "macos-26",
        "fetch-depth: 0",
        "persist-credentials: false",
    )
    missing = [token for token in required if token not in text]
    if mutable or missing or "continue-on-error" in text:
        raise RuntimeError(
            f"embed workflow policy failed: mutable_actions={mutable}, missing={missing}"
        )


def symbol_tool() -> tuple[str, str]:
    try:
        return llvm_tool("llvm-nm"), "--defined-only"
    except RuntimeError:
        if nm := shutil.which("nm"):
            option = "-U" if platform.system() == "Darwin" else "--defined-only"
            return nm, option
        raise


def audit_symbols(executable: Path) -> None:
    nm, option = symbol_tool()
    output = run([nm, option, str(executable)], capture=True).stdout
    symbols = {line.split()[-1] for line in output.splitlines() if len(line.split()) >= 2}
    normalized = {
        symbol[1:]
        if symbol.startswith("_")
        and (CPU_HANDLER.fullmatch(symbol[1:]) or symbol[1:] in FORBIDDEN_SYMBOLS)
        else symbol
        for symbol in symbols
    }
    handlers = {symbol for symbol in normalized if CPU_HANDLER.fullmatch(symbol)}
    forbidden = normalized & FORBIDDEN_SYMBOLS
    print(
        f"embed-link-audit: handlers={len(handlers)}; forbidden={len(forbidden)}",
        flush=True,
    )
    if len(handlers) != 1540 or forbidden:
        raise RuntimeError(
            f"embed link boundary mismatch: handlers={len(handlers)}, forbidden={sorted(forbidden)}"
        )


def main() -> int:
    current = profile()
    build_dir = ROOT / "build" / f"embed-verify-{current.name}"
    verify_workflow()
    run(
        [
            llvm_tool("clang-format"),
            "--style=file",
            "--dry-run",
            "--Werror",
            *(str(source) for source in SOURCES),
        ]
    )
    run(
        [
            "cmake",
            "-S",
            str(EMBED),
            "-B",
            str(build_dir),
            "-G",
            "Ninja",
            "-DCMAKE_BUILD_TYPE=Debug",
            f"-DCMAKE_C_COMPILER={current.c_compiler}",
        ]
    )
    assert_compiler(build_dir, current.compiler_id)
    run(["cmake", "--build", str(build_dir)])
    run(["ctest", "--test-dir", str(build_dir), "--output-on-failure"])
    executable = build_dir / f"uae_m68k_embed_test{current.executable_suffix}"
    audit_symbols(executable)
    run(
        [
            llvm_tool("clang-tidy"),
            f"--config-file={ROOT / '.clang-tidy'}",
            "-p",
            str(build_dir),
            *(str(source) for source in TRANSLATION_UNITS),
        ]
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
