# QUASAR Pneumatic Load Cell Firmware

Firmware and host tools for the automated EEG headset pneumatic load-cell array.

This project streams pressure/force estimates from MPRLS sensors (through TCA9548A muxes) and supports host-side control commands (tare, calibration, hardware reset) over a binary serial protocol.

## Firmware Usage Guide

### 1. What The Firmware Does

- Scans up to 4 TCA9548A muxes at addresses `0x70` to `0x73`.
- Supports up to 32 sensor slots (`4 muxes x 8 channels`).
- Samples every `20 ms` (about `50 Hz`) and streams packets at `921600` baud.
- Uses sensor index `31` as ambient reference for drift compensation.
- Accepts control packets from the host for:
	- `REQUEST_TARING`
	- `REQUEST_CALIBRATION_START`
	- `REQUEST_CALIBRATION_END`
	- `REQUEST_HARDWARE_RESET`

### 2. Hardware/Configuration Assumptions

Current defaults in `include/config.h`:

- I2C pins: `SDA=22`, `SCL=21`
- I2C clock: `400 kHz`
- Serial baud: `921600`
- Ambient sensor index: `31`
- Sampling interval: `20 ms`

`src/main.cpp` initializes I2C with `Wire.begin(SDA_PIN, SCL_PIN)`, so the primary target is `esp32dev`.

### 3. Build And Flash Firmware (PlatformIO)

From the repository root:

```powershell
pio run -e esp32dev
pio run -e esp32dev -t upload --upload-port COMx
```

Replace `COMx` with your board port (for example `COM5`).

If you use VS Code + PlatformIO extension, selecting the `esp32dev` environment and clicking Build/Upload does the same thing.

### 4. Open Serial Monitor

```powershell
pio device monitor -e esp32dev --port COMx
```

Expected startup text includes lines like:

- `MPRLS Load Cell Test`
- `Starting live readings...`

### 5. Host Python Setup

The scripts in `scripts/` are the intended host interface for this firmware.

```powershell
python -m venv .venv
.\.venv\Scripts\activate
pip install pyserial pySerialTransfer matplotlib pandas numpy scipy cutie
```

### 6. Stream, Log, And Plot Data

#### A) Log live sensor stream to CSV

```powershell
python scripts/read_mprls_pressure.py
```

- Auto-detects serial ports.
- Saves a timestamped file like `mprls_log_YYYYMMDD_HHMMSS.csv`.
- Prints each parsed sensor packet to console.

Enable live plotting while logging:

```powershell
python scripts/read_mprls_pressure.py --plot
```

#### B) Split a combined CSV into one file per sensor

```powershell
python scripts/extract_individual_sensor_data.py path\to\mprls_log_YYYYMMDD_HHMMSS.csv
```

#### C) Plot existing CSV data

```powershell
python scripts/plot_mprls_csv_live.py path\to\mprls_log_YYYYMMDD_HHMMSS.csv
```

Optional estimator simulation (Deprecated, not in use):

```powershell
python scripts/plot_mprls_csv_live.py path\to\mprls_log_YYYYMMDD_HHMMSS.csv --enable-estimator-pipeline-simulator
```

### 7. Send Control Commands To Firmware

Use the interactive test utility:

```powershell
python scripts/test_packet_communication.py
```

What it does:

- Starts a receiver loop for incoming sensor/control/watchdog packets.
- Starts an interactive menu thread to send control requests by sensor index.

For `REQUEST_CALIBRATION_END`, it prompts for the calibration ratio (float payload).

### 8. Packet Types (Host <-> Firmware)

- `CONTROL` (`0x00`): host command and firmware ACK/error response.
- `SENSOR` (`0x01`): streamed sensor data (`pressure_kPa`, `pressure_rate_kPa_s`, `force_g`).
- `WATCHDOG` (`0x02`): loop-overrun status (`overrun`, `loop_time_ms`).

Control response flags:

- `CTRL_ACK`: command acknowledged.
- `CTRL_BUSY`: sensor busy (for example during calibration/reset).
- `CTRL_ERR`: command rejected or sensor failure.

### 9. Typical Bring-Up Checklist

1. Flash firmware to `esp32dev`.
2. Open monitor at `921600` and verify startup logs.
3. Run `python scripts/read_mprls_pressure.py`.
4. Confirm packets arrive for expected sensor indices.
5. Run `python scripts/test_packet_communication.py` and issue a tare command to a known-good sensor.
6. Please note that the hardware reset for each load cell is currently under development

### 10. Quick Troubleshooting

- No serial port found:
	- Reconnect board and check Device Manager for the COM port.
- No sensor packets but firmware boots:
	- Check TCA9548A addresses and I2C wiring.
	- Confirm the configured SDA/SCL pins match your hardware (`22/21` by default).
- Frequent watchdog overruns:
	- Reduce host-side command flooding.
	- Verify failing sensors are reset/recovered.
- Calibration-end rejected:
	- Send `REQUEST_CALIBRATION_START` first, then `REQUEST_CALIBRATION_END` with payload ratio.

## Acknowledgment

This repository currently depends on:

- Adafruit MPRLS Library
- SerialTransfer
- iir1
- pySerialTransfer