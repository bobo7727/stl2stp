#!/usr/bin/env bash
# Copy the design string tables (single source of truth: design/ui/tools/build_strings.py)
# into the Flutter assets. Run from anywhere.
set -euo pipefail
here="$(cd "$(dirname "$0")/.." && pwd)"
cp "$here/../design/ui/strings_zh.json" "$here/assets/i18n/zh.json"
cp "$here/../design/ui/strings_en.json" "$here/assets/i18n/en.json"
echo "synced $(python3 -c 'import json,sys;print(len(json.load(open(sys.argv[1]))))' "$here/assets/i18n/zh.json") keys"
