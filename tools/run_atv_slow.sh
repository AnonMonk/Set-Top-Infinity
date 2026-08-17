#!/usr/bin/env bash

set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

LIMIT_PERCENT="${1:-10}"
if [[ "${1:-}" =~ ^[0-9]+$ ]]; then
	shift
else
	LIMIT_PERCENT=10
fi

DEMO_BIN="${DEMO_BIN:-./demo}"
if [[ ! -x "$DEMO_BIN" ]]; then
	echo "Binary nicht gefunden: $DEMO_BIN"
	echo "Zuerst bauen:  make mac"
	exit 1
fi

if ! [[ "$LIMIT_PERCENT" =~ ^[0-9]+$ ]] || (( LIMIT_PERCENT < 1 || LIMIT_PERCENT > 100 )); then
	echo "Limit muss 1..100 sein (Prozent einer CPU)."
	exit 1
fi

LOGGER_BIN="${LOGGER_BIN:-$ROOT/tools/log_demo_stats}"
DO_LOG="${NO_CPU_LOG:-0}"
if [[ "$DO_LOG" != "1" ]]; then
	if [[ ! -x "$LOGGER_BIN" ]]; then
		echo "Logger nicht gefunden — baue: make tools"
		make -C "$ROOT" tools
	fi
	if [[ ! -x "$LOGGER_BIN" ]]; then
		echo "Logger-Binary fehlt: $LOGGER_BIN"
		exit 1
	fi
fi

LOG_DIR="${CPU_LOG_DIR:-$ROOT/tools/log}"
mkdir -p "$LOG_DIR"
LOG_STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${CPU_LOG_FILE:-$LOG_DIR/demo_cpu_${LOG_STAMP}.csv}"

sleep_ms() {
	local ms="$1"
	if (( ms <= 0 )); then
		return 0
	fi
	sleep "$(awk -v ms="$ms" 'BEGIN { printf "%.3f", ms / 1000 }')"
}

echo "Starte $DEMO_BIN $*  mit ~${LIMIT_PERCENT}% CPU-Limit"
echo "(extern gebremst — nicht in der Demo eingebaut)"
if [[ "$DO_LOG" != "1" ]]; then
	echo "CPU-Log: $LOG_FILE"
else
	echo "CPU-Log: aus (NO_CPU_LOG=1)"
fi
echo "Beenden: ESC in der Demo oder Ctrl+C hier"
echo

"$DEMO_BIN" "$@" &
DEMO_PID=$!

LOGGER_PID=""
LIMITER_PID=""

cleanup() {
	if [[ -n "${LOGGER_PID}" ]] && kill -0 "$LOGGER_PID" 2>/dev/null; then
		kill "$LOGGER_PID" 2>/dev/null || true
		wait "$LOGGER_PID" 2>/dev/null || true
	fi
	if [[ -n "${LIMITER_PID}" ]] && kill -0 "$LIMITER_PID" 2>/dev/null; then
		kill "$LIMITER_PID" 2>/dev/null || true
	fi
	if kill -0 "$DEMO_PID" 2>/dev/null; then
		kill -CONT "$DEMO_PID" 2>/dev/null || true
		kill "$DEMO_PID" 2>/dev/null || true
		wait "$DEMO_PID" 2>/dev/null || true
	fi
	killall afplay 2>/dev/null || true
}
trap cleanup EXIT INT TERM

if [[ "$DO_LOG" != "1" ]]; then
	"$LOGGER_BIN" "$DEMO_PID" 0.5 "$LOG_FILE" &
	LOGGER_PID=$!
fi

if command -v cpulimit >/dev/null 2>&1; then
	echo "Limiter: cpulimit -l $LIMIT_PERCENT"
	cpulimit -l "$LIMIT_PERCENT" -p "$DEMO_PID" &
	LIMITER_PID=$!
	wait "$DEMO_PID" || true
	if [[ -n "${LOGGER_PID}" ]]; then
		wait "$LOGGER_PID" 2>/dev/null || true
	fi
	if [[ "$DO_LOG" != "1" ]]; then
		echo "Log gespeichert: $LOG_FILE"
	fi
	exit 0
fi

echo "Limiter: eingebauter Duty-Cycle (cpulimit nicht gefunden)"
echo "Tipp fuer praeziseres Limit:  brew install cpulimit"

CYCLE_MS=100
RUN_MS=$(( CYCLE_MS * LIMIT_PERCENT / 100 ))
if (( RUN_MS < 1 )); then RUN_MS=1; fi
SLEEP_MS=$(( CYCLE_MS - RUN_MS ))
if (( SLEEP_MS < 0 )); then SLEEP_MS=0; fi

taskpolicy -b -p "$DEMO_PID" 2>/dev/null || true

(
	while kill -0 "$DEMO_PID" 2>/dev/null; do
		if (( SLEEP_MS > 0 )); then
			kill -CONT "$DEMO_PID" 2>/dev/null || exit 0
			sleep_ms "$RUN_MS"
			kill -STOP "$DEMO_PID" 2>/dev/null || exit 0
			sleep_ms "$SLEEP_MS"
		else
			sleep 0.05
		fi
	done
) &
LIMITER_PID=$!

wait "$DEMO_PID" || true
kill -CONT "$DEMO_PID" 2>/dev/null || true
if [[ -n "${LOGGER_PID}" ]]; then
	wait "$LOGGER_PID" 2>/dev/null || true
fi
if [[ "$DO_LOG" != "1" ]]; then
	echo "Log gespeichert: $LOG_FILE"
fi
exit 0
