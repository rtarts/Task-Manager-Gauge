# Arduino PC Task Manager Dashboard

A small hardware dashboard that shows your PC's live **CPU, RAM and GPU usage** on a 128x64 OLED. A Python script on the PC reads system stats and streams them over USB serial to an Arduino UNO R4 Minima, which draws them as three animated bars. If any metric hits 99% or more, the display switches to a glitching "MAXED!!" overload warning.

## Features

- Live CPU, RAM and GPU readings, refreshed about once per second
- Smooth, eased bar animation on the OLED
- "Waiting for PC..." screen when the PC script isn't running or stops sending (4 s timeout)
- Overload mode at 99%+: shaking gauge, noise, flashing warning and inverted display
- Robust serial parsing: ignores malformed or oversized lines
- GPU usage works with any GPU vendor on Windows 10/11 (no vendor-specific tools)

## How it works

```
PC (Python)                          Arduino UNO R4 Minima
-----------                          ---------------------
psutil  -> CPU %, RAM %   --USB-->   parse "cpu,ram,gpu\n"
PowerShell Get-Counter -> GPU %      ease values, draw bars on SSD1306 OLED
```

**Serial protocol** (115200 baud, once per second):

```
<cpu>,<ram>,<gpu>\n        e.g.  42,67,55
```

GPU usage is read from the same Windows "GPU Engine" performance counters that Task Manager's GPU graph uses (the `engtype_3D` instances, summed and clamped to 0-100).

## Hardware

| Part | Notes |
|------|-------|
| Arduino UNO R4 Minima | Connects to the PC over USB |
| SSD1306 128x64 OLED (I2C) | Default I2C address `0x3C` (some boards use `0x3D`) |
| Jumper wires | OLED VCC, GND, SDA, SCL to the matching Arduino pins |

## Software requirements

**Arduino (libraries via Library Manager):**
- Adafruit GFX Library
- Adafruit SSD1306

**PC (Python 3):**
```bash
pip install psutil pyserial
```

## Setup

1. Wire the OLED to the Arduino's I2C pins (SDA/SCL), plus 3.3V/5V and GND per your display's spec.
2. Open `task_manager_gauge.ino` in the Arduino IDE, select the **Arduino UNO R4 Minima** board and upload.
3. Find the Arduino's COM port: **Device Manager > Ports (COM & LPT)**.
4. In `task_manager_sender.py`, set `PORT` (for example `PORT = "COM6"`) to match.
5. Run the sender:
   ```bash
   python task_manager_sender.py
   ```
6. The OLED switches from "Waiting for PC..." to the live dashboard.

Press `Ctrl+C` to stop the sender.

## Configuration

| Setting | File | Default | Notes |
|---------|------|---------|-------|
| `PORT` | `task_manager_sender.py` | `COM6` | Your Arduino's serial port |
| `BAUD` | both files (`.py` and `.ino`) | `115200` | Must match in the script and the sketch |
| `SEND_INTERVAL_SEC` | `task_manager_sender.py` | `1.0` | Update rate |
| `SCREEN_ADDR` | `task_manager_gauge.ino` | `0x3C` | Change to `0x3D` if the OLED isn't detected |
| `SIGNAL_TIMEOUT_MS` | `task_manager_gauge.ino` | `4000` | Time before "Waiting for PC..." appears |

## Troubleshooting

- **Could not open COM port:** wrong `PORT`, or another program (e.g. the Arduino Serial Monitor) is holding it. Close it and retry.
- **Screen stays blank:** the sketch halts if the OLED isn't found. Check wiring and try address `0x3D`.
- **GPU always shows 0%:** the counter may be unavailable. Confirm GPU activity appears in Windows Task Manager.
- **Linux/macOS:** GPU reading is Windows-only. Replace `get_gpu_percent()` with another source (for example `radeontop` for AMD on Linux).

## Files

| File | Purpose |
|------|---------|
| `task_manager_gauge.ino` | Arduino sketch: receives data and draws the dashboard |
| `task_manager_sender.py` | PC script: collects CPU/RAM/GPU usage and sends it over serial |
