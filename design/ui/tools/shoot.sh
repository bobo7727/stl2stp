#!/bin/bash
# usage: shoot.sh out.png "query" "hash" [w h]
D=$(cd "$(dirname "$0")/.." && pwd)
W=${4:-393}; H=${5:-852}
google-chrome --headless=new --no-sandbox --disable-gpu --hide-scrollbars --force-device-scale-factor=2 \
  --window-size=$W,$H --virtual-time-budget=1500 --screenshot="$1" "file://$D/prototype/index.html?$2#/$3" >/dev/null 2>&1
