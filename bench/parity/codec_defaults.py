#!/usr/bin/env python3
"""Extract the default values a RecordCodecBuilder supplies for missing fields.

The 26.2 worldgen JSON omits optional fields for some features (the geode's
`distribution_points`, `layers` contents and `point_offset` are simply absent),
so the values the reference server actually used come from the codec defaults in
the class file. This prints each optionalFieldOf name with its default and the
constant pool entry it came from, so the C port can be given the same defaults.

    python3 bench/parity/codec_defaults.py <class/path/Name.class>
"""
from __future__ import annotations

import re
import subprocess
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


def main() -> int:
    if len(sys.argv) < 2:
        raise SystemExit(__doc__)
    cls = sys.argv[1]
    out = subprocess.run(
        ["bash", str(HERE / "javap26.sh"), cls], capture_output=True, text=True, check=True
    ).stdout

    # javap prints the constants on the lines between the field-name ldc and the
    # optionalFieldOf call; walk the disassembly in order.
    lines = out.splitlines()
    pending: list[tuple[str, str]] = []
    for line in lines:
        m = re.search(r'// String ([a-z0-9_]+)$', line)
        if m and 'optionalFieldOf' not in line:
            pending.append((m.group(1), ''))
            continue
        m = re.search(r'// (?:double|float|int|long) ([0-9.eE+-]+)', line)
        if m and pending:
            name, _ = pending[-1]
            pending[-1] = (name, m.group(1))
            continue
        if 'PrimitiveCodec' in line or 'Integer.valueOf' in line or 'Boolean.valueOf' in line:
            if pending and not pending[-1][1]:
                m2 = re.search(r'iconst_(\d)', line)
                if m2:
                    name, _ = pending[-1]
                    pending[-1] = (name, m2.group(1))
            continue
        if 'optionalFieldOf' in line:
            if pending:
                name, value = pending.pop()
                print(f"{name:42s} default={value or '(see bytecode)'}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
