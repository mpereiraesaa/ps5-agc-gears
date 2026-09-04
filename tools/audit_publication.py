#!/usr/bin/env python3
"""Fail-closed audit for the future standalone public repository."""

from __future__ import annotations

import ipaddress
import re
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
ALLOWLIST = ROOT / "PUBLICATION_ALLOWLIST.txt"
TEXT_SUFFIXES = {"", ".json", ".md", ".py", ".txt"}
PINNED_BINARY_SHA256 = {
    "assets/branding/icon-master.png":
        "50accc91e38822a8b11cb6eed916d968184edb8306a1099fc3d2aa0a72b402b0",
    "sce_sys/icon0.png":
        "cc40f50deb429e8bcf07eb43be5a3176c4f8445a88e045e830b066202b66efb8",
}
FORBIDDEN_PARTS = {
    ".deps", "build", "captures", "dist", "dumps", "ghidra", "sessions",
}
FORBIDDEN_TERMS = (
    "/home/" + "manuel/",
    "/data/homebrew/" + "PPSA99998",
    "authorized-graphics-" + "contract",
    "stage-e-live-" + "last.log",
)


def fail(message: str) -> None:
    raise SystemExit(f"publication audit failed: {message}")


def main() -> int:
    allowed = {
        line.strip() for line in ALLOWLIST.read_text().splitlines()
        if line.strip() and not line.lstrip().startswith("#")
    }
    observed: set[str] = set()
    for path in ROOT.rglob("*"):
        relative = path.relative_to(ROOT)
        if ".git" in relative.parts:
            continue
        if any(part in FORBIDDEN_PARTS or part == "__pycache__"
               for part in relative.parts):
            fail(f"forbidden path: {relative}")
        if path.is_symlink():
            fail(f"symlink is not publishable: {relative}")
        if not path.is_file():
            continue
        name = relative.as_posix()
        observed.add(name)
        if name not in allowed:
            fail(f"file is not allowlisted: {name}")
        if name in PINNED_BINARY_SHA256:
            import hashlib
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest != PINNED_BINARY_SHA256[name]:
                fail(f"binary digest changed: {name}")
            continue
        if path.suffix.lower() not in TEXT_SUFFIXES:
            fail(f"non-text file is not approved: {name}")
        try:
            text = path.read_text(encoding="utf-8")
        except UnicodeDecodeError:
            fail(f"non-UTF-8 content: {name}")
        for term in FORBIDDEN_TERMS:
            if term in text:
                fail(f"private term in {name}: {term}")
        for candidate in re.findall(r"(?<![\w.])(?:\d{1,3}\.){3}\d{1,3}(?![\w.])", text):
            try:
                address = ipaddress.ip_address(candidate)
            except ValueError:
                continue
            if address.is_private:
                fail(f"private IP address in {name}")
    missing = allowed - observed
    if missing:
        fail(f"allowlisted files missing: {', '.join(sorted(missing))}")
    print(f"publication audit passed: {len(observed)} allowlisted files")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
