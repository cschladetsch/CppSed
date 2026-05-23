#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
import time


def main() -> int:
    if len(sys.argv) < 2:
        print("usage: time_command.py COMMAND...", file=sys.stderr)
        return 1

    started = time.perf_counter()
    subprocess.run(sys.argv[1:], stdout=subprocess.DEVNULL, check=True)
    elapsed = time.perf_counter() - started
    print(f"{elapsed:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
