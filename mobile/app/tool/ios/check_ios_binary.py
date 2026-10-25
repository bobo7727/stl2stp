#!/usr/bin/env python3
"""Fail a Mac/iOS build if Runner does not export every public core function."""
import argparse
from pathlib import Path
import subprocess
import sys

from core_symbols import checked_symbols


def missing_exports(nm_output, required):
    defined = {line.strip().lstrip('_') for line in nm_output.splitlines() if line.strip()}
    return sorted(set(required) - defined)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('binary', type=Path)
    args = parser.parse_args()
    mobile = Path(__file__).resolve().parents[3]
    try:
        if not args.binary.is_file():
            raise ValueError('Runner binary does not exist: ' + str(args.binary))
        required = checked_symbols(
            (mobile / 'core/include/stl2stp/stl2stp.h').read_text(encoding='utf-8'),
            (mobile / 'app/lib/core/ffi/bindings.dart').read_text(encoding='utf-8'),
        )
        output = subprocess.run(['xcrun', 'nm', '-gjU', str(args.binary)],
                                check=True, capture_output=True, text=True).stdout
        missing = missing_exports(output, required)
        if missing:
            raise ValueError('Runner is missing C API exports: ' + ', '.join(missing))
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print('iOS binary validation failed: ' + str(error), file=sys.stderr)
        return 1
    print(f'Runner binary validated: all {len(required)} public C API exports are present.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
