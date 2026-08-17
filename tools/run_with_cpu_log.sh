#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

DEMO_BIN="${DEMO_BIN:-./demo}"
if [[ ! -x "$DEMO_BIN" ]]; then
	echo "Binary nicht gefunden: $DEMO_BIN — zuerst: make mac"
	exit 1
fi

LOGGER_BIN="${LOGGER_BIN:-$ROOT/tools/log_demo_stats}"
if [[ ! -x "$LOGGER_BIN" ]]; then
	echo "Logger nicht gefunden — baue: make tools"
	make -C "$ROOT" tools
fi
if [[ ! -x "$LOGGER_BIN" ]]; then
	echo "Logger-Binary fehlt: $LOGGER_BIN"
	exit 1
fi

LOG_DIR="${CPU_LOG_DIR:-$ROOT/tools/log}"
mkdir -p "$LOG_DIR"
LOG_STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${CPU_LOG_FILE:-$LOG_DIR/demo_cpu_${LOG_STAMP}.csv}"

echo "Starte $DEMO_BIN $*  (volle CPU)"
echo "CPU-Log: $LOG_FILE"
echo "Beenden: ESC oder Ctrl+C"
echo

"$DEMO_BIN" "$@" &
DEMO_PID=$!

cleanup() {
	if kill -0 "$LOGGER_PID" 2>/dev/null; then
		kill "$LOGGER_PID" 2>/dev/null || true
		wait "$LOGGER_PID" 2>/dev/null || true
	fi
	if kill -0 "$DEMO_PID" 2>/dev/null; then
		kill "$DEMO_PID" 2>/dev/null || true
		wait "$DEMO_PID" 2>/dev/null || true
	fi
	killall afplay 2>/dev/null || true
	echo "Log gespeichert: $LOG_FILE"
}
trap cleanup EXIT INT TERM

"$LOGGER_BIN" "$DEMO_PID" 0.5 "$LOG_FILE" &
LOGGER_PID=$!

wait "$DEMO_PID" || true
wait "$LOGGER_PID" 2>/dev/null || true
