#!/usr/bin/env bash
# Startet die Demo abgebremst (ATV1-Näherung). Aendert den Demo-Code nicht.
# Schreibt optional CPU-Log nach tools/log/demo_cpu_*.csv (Standard: an).
#
# Usage:
#   ./tools/run_atv_slow.sh              # ~10 % CPU + Log
#   ./tools/run_atv_slow.sh 5            # ~5 %
#   ./tools/run_atv_slow.sh 15 mandelbrot
#   NO_CPU_LOG=1 ./tools/run_atv_slow.sh # ohne Log
#
# Optional (praeziseres Limit):  brew install cpulimit

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

LOG_DIR="${CPU_LOG_DIR:-$ROOT/tools/log}"
mkdir -p "$LOG_DIR"
LOG_STAMP="$(date +%Y%m%d_%H%M%S)"
LOG_FILE="${CPU_LOG_FILE:-$LOG_DIR/demo_cpu_${LOG_STAMP}.csv}"
DO_LOG="${NO_CPU_LOG:-0}"

echo "Starte $DEMO_BIN $*  mit ~${LIMIT_PERCENT}% CPU-Limit"
echo "(extern gebremst — nicht in der Demo eingebaut)"
if [[ "$DO_LOG" != "1" ]]; then
	echo "CPU-Log: $LOG_FILE"
else
	echo "CPU-Log: aus (NO_CPU_LOG=1)"
fi
echo "Beenden: ESC in der Demo oder Ctrl+C hier"
echo

# Demo starten
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
	# verwaiste Musik beenden (macOS)
	killall afplay 2>/dev/null || true
}
trap cleanup EXIT INT TERM

# Logger starten (extern, alle 0.5 s)
if [[ "$DO_LOG" != "1" ]]; then
	python3 "$ROOT/tools/log_demo_stats.py" "$DEMO_PID" 0.5 "$LOG_FILE" &
	LOGGER_PID=$!
fi

# --- Variante 1: cpulimit (falls installiert) ---
if command -v cpulimit >/dev/null 2>&1; then
	echo "Limiter: cpulimit -l $LIMIT_PERCENT"
	cpulimit -l "$LIMIT_PERCENT" -p "$DEMO_PID" &
	LIMITER_PID=$!
	wait "$DEMO_PID" || true
	EXIT_CODE=0
	if [[ -n "${LOGGER_PID}" ]]; then
		wait "$LOGGER_PID" 2>/dev/null || true
	fi
	if [[ "$DO_LOG" != "1" ]]; then
		echo "Log gespeichert: $LOG_FILE"
	fi
	exit 0
fi

# --- Variante 2: duty-cycle mit SIGSTOP/SIGCONT ---
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
			python3 - "$RUN_MS" <<'PY'
import sys, time
time.sleep(int(sys.argv[1]) / 1000.0)
PY
			kill -STOP "$DEMO_PID" 2>/dev/null || exit 0
			python3 - "$SLEEP_MS" <<'PY'
import sys, time
time.sleep(int(sys.argv[1]) / 1000.0)
PY
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
