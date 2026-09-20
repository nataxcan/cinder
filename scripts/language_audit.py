#!/usr/bin/env python3
"""What in this repo is not Bend?

Walks the tree, classifies every source file by language, and prints the totals
plus the foreign files the .bend sources pull in through `import`, so the share of
the project that is Bend (and the share that is not) is a number rather than an
impression.

    python3 scripts/language_audit.py
"""
from __future__ import annotations

import pathlib
import re
import subprocess
from collections import defaultdict

ROOT = pathlib.Path(__file__).resolve().parents[1]

LANGS = {
    ".bend": "Bend",
    ".c": "C",
    ".h": "C headers",
    ".js": "JavaScript",
    ".py": "Python (tooling)",
    ".sh": "Shell (tooling)",
    ".md": "Markdown",
    ".html": "HTML",
    ".json": "JSON",
    ".css": "CSS",
}
SKIP_DIRS = {".git", "bench/pumpkin", "bench/folia", "bench/c2me-run", "__pycache__", "logs"}


def tracked() -> list[str]:
    out = subprocess.run(
        ["git", "ls-files"], cwd=ROOT, capture_output=True, text=True, check=True
    ).stdout.split()
    return [p for p in out if not any(p.startswith(d + "/") or p == d for d in SKIP_DIRS)]


def main() -> int:
    by_lang: dict[str, list[tuple[str, int]]] = defaultdict(list)
    for rel in tracked():
        path = ROOT / rel
        ext = pathlib.Path(rel).suffix.lower()
        lang = LANGS.get(ext)
        if lang is None:
            continue
        try:
            lines = len(path.read_text(encoding="utf-8", errors="replace").splitlines())
        except OSError:
            continue
        by_lang[lang].append((rel, lines))

    total = sum(n for v in by_lang.values() for _, n in v)
    print(f"{'language':22s} {'files':>6s} {'lines':>8s}  share")
    for lang, files in sorted(by_lang.items(), key=lambda kv: -sum(n for _, n in kv[1])):
        lines = sum(n for _, n in files)
        print(f"{lang:22s} {len(files):>6d} {lines:>8d}  {100 * lines / total:5.1f}%")
    print(f"{'TOTAL':22s} {sum(len(v) for v in by_lang.values()):>6d} {total:>8d}")

    print("\nforeign imports from the .bend sources:")
    for path in sorted(ROOT.glob("src/*.bend")):
        text = path.read_text(encoding="utf-8", errors="replace")
        for m in re.finditer(r'import\s+"\./([A-Za-z0-9_.]+)"', text):
            target = m.group(1)
            if target.endswith(".bend"):
                continue
            lines = (ROOT / "src" / target).read_text(errors="replace").count("\n")
            print(f"  {path.name:14s} -> {target:26s} {lines:>6d} lines")

    print("\nthe C worldgen, by module:")
    for path in sorted((ROOT / "src").glob("vanilla_*.c"), key=lambda p: -p.stat().st_size):
        print(f"  {path.name:32s} {path.read_text(errors='replace').count(chr(10)):>6d} lines")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
