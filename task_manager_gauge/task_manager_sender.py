"""
task_manager_sender.py
------------------------
Reads live CPU / RAM / GPU usage from your PC and streams it to the
Arduino over USB serial, once per second, in the format:

    "<cpu>,<ram>,<gpu>\n"

Pairs with: task_manager_gauge.ino

GPU usage (AMD, Windows): read via the same "GPU Engine" performance
counters that Windows Task Manager's GPU graph uses (Get-Counter on
the 3D engine instances, summed). This works for any GPU vendor on
Windows 10/11 - no vendor-specific tool needed - but it IS
Windows-only. If you're on Linux, swap get_gpu_percent() for a
`radeontop` subprocess call instead; happy to write that version if
you end up there.

Setup:
    pip install psutil pyserial

Then set PORT below to your Arduino's serial port and run:
    python task_manager_sender.py

Finding your port:
    Windows  -> Device Manager > Ports (COM & LPT), look for "Arduino"
                or "USB Serial Device" (e.g. "COM5")
"""

import re
import subprocess
import sys
import time

import psutil
import serial

PORT = "COM6"          # <-- CHANGE THIS to match your Arduino's port
BAUD = 115200           # must match Serial.begin() in the .ino file
SEND_INTERVAL_SEC = 1.0


def get_gpu_percent():
    """
    Reads overall GPU 3D-engine utilization via Windows performance
    counters (same data source as Task Manager's GPU graph). Sums all
    "engtype_3D" instances across all GPUs/processes, clamped to 0-100.
    Returns 0.0 if the counter is unavailable for any reason.
    """
    ps_cmd = (
        "(Get-Counter '\\GPU Engine(*engtype_3D)\\Utilization Percentage' "
        "-ErrorAction SilentlyContinue).CounterSamples "
        "| Measure-Object -Property CookedValue -Sum "
        "| Select-Object -ExpandProperty Sum"
    )
    try:
        result = subprocess.run(
            ["powershell", "-NoProfile", "-Command", ps_cmd],
            capture_output=True, text=True, timeout=5,
        )
        match = re.search(r"[\d.]+", result.stdout)
        if not match:
            return 0.0
        return max(0.0, min(100.0, float(match.group())))
    except Exception:
        return 0.0


def main():
    print(f"Connecting to {PORT} @ {BAUD} baud...")
    try:
        ser = serial.Serial(PORT, BAUD, timeout=1)
    except serial.SerialException as e:
        print(f"Could not open {PORT}: {e}")
        print("Check the PORT variable at the top of this script.")
        sys.exit(1)

    # Arduino resets when a serial connection opens - give it a moment to boot
    time.sleep(2)
    print("Connected. Streaming CPU / RAM / GPU usage... (Ctrl+C to stop)")

    try:
        while True:
            cpu = psutil.cpu_percent(interval=0.5)       # blocks ~0.5s, doubles as our sample window
            ram = psutil.virtual_memory().percent
            gpu = get_gpu_percent()

            line = f"{cpu:.0f},{ram:.0f},{gpu:.0f}\n"
            ser.write(line.encode("utf-8"))
            print(f"Sent: {line.strip()}")

            time.sleep(max(0, SEND_INTERVAL_SEC - 0.5))
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        ser.close()


if __name__ == "__main__":
    main()

