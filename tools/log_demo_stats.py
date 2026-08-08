#!/usr/bin/env python3
"""
Loggt waehrend die Demo laeuft CPU/RAM in eine CSV unter tools/log/.
Aendert den Demo-Code nicht — externer Begleitprozess.

Usage:
  ./tools/log_demo_stats.py <demo_pid> [interval_sec] [logfile]
  ./tools/log_demo_stats.py --find-demo

CSV-Spalten:
  time_iso, elapsed_s, demo_cpu_pct, demo_rss_mb,
  load_1m, load_5m, load_15m, ncpu,
  core0_pct, core1_pct, ... (System-Kerne, nicht "welcher Kern die Demo nutzt")
"""

from __future__ import annotations

import csv
import ctypes
import ctypes.util
import os
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


TOOLS_DIR = Path(__file__).resolve().parent
LOG_DIR = TOOLS_DIR / "log"
DEFAULT_INTERVAL = 0.5

# Mach CPU states
CPU_STATE_USER = 0
CPU_STATE_SYSTEM = 1
CPU_STATE_IDLE = 2
CPU_STATE_NICE = 3
PROCESSOR_CPU_LOAD_INFO = 2


class processor_cpu_load_info(ctypes.Structure):
    _fields_ = [("cpu_ticks", ctypes.c_uint * 4)]


def _mach_cpu_ticks():
    """Pro Kern: (user, system, idle, nice) Tick-Zaehler."""
    libc = ctypes.CDLL(ctypes.util.find_library("c"), use_errno=True)
    host = libc.mach_host_self()
    count = ctypes.c_uint(0)
    info_ptr = ctypes.c_void_p()
    cpu_count = ctypes.c_uint(0)

    # host_processor_info(host, flavor, *out_count_cpus, **info, *info_count)
    libc.host_processor_info.argtypes = [
        ctypes.c_uint,
        ctypes.c_int,
        ctypes.POINTER(ctypes.c_uint),
        ctypes.POINTER(ctypes.c_void_p),
        ctypes.POINTER(ctypes.c_uint),
    ]
    libc.host_processor_info.restype = ctypes.c_int

    kr = libc.host_processor_info(
        host,
        PROCESSOR_CPU_LOAD_INFO,
        ctypes.byref(cpu_count),
        ctypes.byref(info_ptr),
        ctypes.byref(count),
    )
    if kr != 0 or not info_ptr.value:
        return None

    n = int(cpu_count.value)
    # count is total natural_t values = n * 4
    arr_type = processor_cpu_load_info * n
    arr = ctypes.cast(info_ptr, ctypes.POINTER(arr_type)).contents

    ticks = []
    for i in range(n):
        t = arr[i].cpu_ticks
        ticks.append((int(t[0]), int(t[1]), int(t[2]), int(t[3])))

    # vm_deallocate
    try:
        libc.vm_deallocate(
            libc.mach_task_self(),
            ctypes.c_ulong(info_ptr.value),
            ctypes.c_ulong(count.value * ctypes.sizeof(ctypes.c_uint)),
        )
    except Exception:
        pass

    return ticks


def core_usage_pct(prev, cur):
    """Aus zwei Tick-Samples prozentuale Nicht-Idle-Last pro Kern."""
    if prev is None or cur is None or len(prev) != len(cur):
        return None
    out = []
    for a, b in zip(prev, cur):
        du = b[0] - a[0]
        ds = b[1] - a[1]
        di = b[2] - a[2]
        dn = b[3] - a[3]
        total = du + ds + di + dn
        if total <= 0:
            out.append(0.0)
        else:
            busy = du + ds + dn
            out.append(100.0 * busy / total)
    return out


def read_loadavg():
    try:
        return os.getloadavg()  # 1, 5, 15
    except OSError:
        return (0.0, 0.0, 0.0)


def read_process(pid: int):
    """(cpu_percent_since_boot_style via ps, rss_mb) — ps %cpu ist 'recent' auf macOS."""
    # ps: %cpu and rss (KB on macOS)
    import subprocess

    try:
        out = subprocess.check_output(
            ["ps", "-p", str(pid), "-o", "%cpu=", "-o", "rss="],
            text=True,
            stderr=subprocess.DEVNULL,
        ).strip()
    except subprocess.CalledProcessError:
        return None
    if not out:
        return None
    parts = out.split()
    if len(parts) < 2:
        return None
    try:
        cpu = float(parts[0].replace(",", "."))
        rss_kb = float(parts[1].replace(",", "."))
    except ValueError:
        return None
    return cpu, rss_kb / 1024.0


def find_demo_pid():
    import subprocess

    try:
        out = subprocess.check_output(
            ["pgrep", "-x", "demo"], text=True, stderr=subprocess.DEVNULL
        ).strip()
    except subprocess.CalledProcessError:
        return None
    if not out:
        return None
    # nimm den neuesten (letzte Zeile oft ok; pgrep listet mehrere)
    pids = [int(x) for x in out.split() if x.isdigit()]
    return pids[-1] if pids else None


def default_log_path() -> Path:
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    LOG_DIR.mkdir(parents=True, exist_ok=True)
    return LOG_DIR / f"demo_cpu_{stamp}.csv"


def main():
    args = sys.argv[1:]
    pid = None
    interval = DEFAULT_INTERVAL
    log_path = None

    if not args or args[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    if args[0] == "--find-demo":
        # warte bis demo da ist
        print("Warte auf Prozess 'demo' …", flush=True)
        for _ in range(600):
            pid = find_demo_pid()
            if pid:
                break
            time.sleep(0.1)
        if not pid:
            print("Kein demo-Prozess gefunden.", file=sys.stderr)
            return 1
        args = args[1:]
    else:
        try:
            pid = int(args[0])
        except ValueError:
            print("Erste Argument: PID oder --find-demo", file=sys.stderr)
            return 1
        args = args[1:]

    if args:
        try:
            interval = float(args[0])
            args = args[1:]
        except ValueError:
            pass

    if args:
        log_path = Path(args[0])
    else:
        log_path = default_log_path()

    ncpu = os.cpu_count() or 1
    prev_ticks = _mach_cpu_ticks()
    time.sleep(0.15)
    start = time.time()

    # Header: feste + core-Spalten
    core_headers = [f"core{i}_pct" for i in range(ncpu)]
    fieldnames = [
        "time_iso",
        "elapsed_s",
        "demo_pid",
        "demo_cpu_pct",
        "demo_rss_mb",
        "load_1m",
        "load_5m",
        "load_15m",
        "ncpu",
    ] + core_headers

    log_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"Log: {log_path}", flush=True)
    print(f"PID: {pid}  interval: {interval}s  ncpu: {ncpu}", flush=True)

    with log_path.open("w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        f.flush()

        while True:
            if not _pid_alive(pid):
                break

            proc = read_process(pid)
            if proc is None:
                break
            demo_cpu, demo_rss = proc

            cur_ticks = _mach_cpu_ticks()
            cores = core_usage_pct(prev_ticks, cur_ticks)
            prev_ticks = cur_ticks if cur_ticks is not None else prev_ticks

            load1, load5, load15 = read_loadavg()
            now = time.time()
            row = {
                "time_iso": datetime.now(timezone.utc).astimezone().isoformat(timespec="milliseconds"),
                "elapsed_s": f"{now - start:.2f}",
                "demo_pid": pid,
                "demo_cpu_pct": f"{demo_cpu:.1f}",
                "demo_rss_mb": f"{demo_rss:.1f}",
                "load_1m": f"{load1:.2f}",
                "load_5m": f"{load5:.2f}",
                "load_15m": f"{load15:.2f}",
                "ncpu": ncpu,
            }
            if cores is not None:
                for i, pct in enumerate(cores):
                    if i < ncpu:
                        row[f"core{i}_pct"] = f"{pct:.1f}"
            for i in range(ncpu):
                row.setdefault(f"core{i}_pct", "")

            writer.writerow(row)
            f.flush()
            time.sleep(interval)

    print(f"Log beendet: {log_path}", flush=True)
    return 0


def _pid_alive(pid: int) -> bool:
    try:
        os.kill(pid, 0)
        return True
    except OSError:
        return False


if __name__ == "__main__":
    sys.exit(main())
