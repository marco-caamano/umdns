#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT_DIR"

printf "[smoke] checking --help output\n"
./bin/umdns_server --help >/dev/null
./bin/umdns_client --help >/dev/null
./bin/umdns_browser --help >/dev/null

printf "[smoke] launching server and running hostname query\n"
./bin/umdns_server -c ./config/umdns_server.ini --log-level error >/tmp/umdns_server_smoke.log 2>&1 &
SERVER_PID=$!
cleanup() {
  kill "$SERVER_PID" >/dev/null 2>&1 || true
}
trap cleanup EXIT

sleep 1
./bin/umdns_client -H umdns-node -t 1 --log-level error >/tmp/umdns_client_smoke.log 2>&1 || true

printf "[smoke] launching finite browser cycle\n"
./bin/umdns_browser -t 1 -n 1 --log-level error >/tmp/umdns_browser_smoke.log 2>&1 || true

printf "[smoke] complete\n"
