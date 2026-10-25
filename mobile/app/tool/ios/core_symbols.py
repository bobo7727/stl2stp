#!/usr/bin/env python3
"""Check Dart's C API lookups and emit iOS linker retention flags.

Run without arguments to check on any platform; --linker-flags is used by
build_ios_core.sh. The public C header is the only export list to maintain.
"""

import argparse
from pathlib import Path
import re
import sys


def public_symbols(header):
    # This header contains declarations only. Strip C comments so examples do
    # not become exports; callback typedef names are followed by ')', not '('.
    declarations = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.S)
    symbols = set(re.findall(r"\b(stl2stp_[A-Za-z0-9_]+)\s*\(", declarations))
    if not symbols:
        raise ValueError("no public stl2stp functions found in the C header")
    return symbols


def checked_symbols(header, bindings):
    symbols = public_symbols(header)
    lookups = set(re.findall(r"['\"](stl2stp_[A-Za-z0-9_]+)['\"]", bindings))
    if not lookups:
        raise ValueError("no stl2stp function lookups found in the Dart bindings")
    unknown = lookups - symbols
    if unknown:
        raise ValueError("Dart looks up functions absent from the C header: " + ", ".join(sorted(unknown)))
    return sorted(symbols)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--linker-flags", action="store_true")
    args = parser.parse_args()
    mobile = Path(__file__).resolve().parents[3]
    try:
        symbols = checked_symbols(
            (mobile / "core/include/stl2stp/stl2stp.h").read_text(encoding="utf-8"),
            (mobile / "app/lib/core/ffi/bindings.dart").read_text(encoding="utf-8"),
        )
    except (OSError, ValueError) as error:
        print("C API validation failed: " + str(error), file=sys.stderr)
        return 1
    if args.linker_flags:
        print(" ".join("-Wl,-u,_" + symbol for symbol in symbols))
    else:
        print(f"C API validation passed: {len(symbols)} public functions retained; Dart lookups covered.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
