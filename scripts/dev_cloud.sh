#!/usr/bin/env bash
# Start the cloud API and the device simulator on this laptop.
set -e
cd "$(dirname "$0")/.."
if [ ! -d server ]; then echo "run from the repo root"; exit 1; fi
python3 server/app.py &
API=$!
sleep 2
python3 tools/sim_device.py --fire 25 &
SIM=$!
trap 'kill $API $SIM 2>/dev/null' INT TERM
echo "API  http://127.0.0.1:8000"
echo "open http://127.0.0.1:8000 in a browser; Ctrl-C stops both"
wait
