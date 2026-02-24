"""Read MPRLS pressure values from serial, log to CSV and plot live.

This replaces the previous simple logger with a live matplotlib plot while
continuing to append timestamped values to a CSV file.
"""

import re
import time
import csv
from datetime import datetime
from collections import deque

import serial
from serial.tools import list_ports
import matplotlib.pyplot as plt
import argparse

BAUD = 115200
KEY = ">"


def autodetect_port():
    ports = list(list_ports.comports())
    if not ports:
        raise RuntimeError("No serial ports found.")
    if len(ports) == 1:
        return ports[0].device

    print("Available ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device}  {p.description}")
    idx = int(input("Select port index: "))
    return ports[idx].device


def main(no_plot=False):
    port = autodetect_port()
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_path = f"mprls_log_{ts}.csv"

    ser = serial.Serial(port, BAUD, timeout=0.5)
    time.sleep(2.0)  # device often resets when the port opens

    # live-plot buffers (store recent samples)
    max_samples = 300
    times = deque(maxlen=max_samples)
    pressures = deque(maxlen=max_samples)

    if not no_plot:
        plt.ion()
        fig, ax = plt.subplots()
        line, = ax.plot([], [], '-o', markersize=4)
        ax.set_xlabel('Elapsed time (s)')
        ax.set_ylabel('Pressure (kPa)')
        ax.grid(True)

    t0 = time.time()
    # for sample-rate estimation
    last_unix_with_idx = {}
    dt_samples_with_idx = {}

    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_unix_s", "sensor_idx", "pressure_kpa", "pressure_psi", "weight_g", "pressure_rate_kpa"])  # header

        try:
            while True:
                raw = ser.readline().decode(errors="ignore").strip()
                if not raw:
                    # keep loop idle-friendly; if plotting is enabled, pump GUI events
                    if not no_plot:
                        plt.pause(0.001)
                    else:
                        time.sleep(0.001)
                    continue

                if raw.startswith(KEY):
                    m = re.search(r"Pressure_kPa_(\d+):([-+]?\d*\.?\d+)", raw)
                    # allow lines that include pressure but maybe also other fields
                    if not m:
                        continue

                    sensor_idx = int(m.group(1))

                    # optional weight field
                    mw = re.search(fr"Detected_weight_g_{sensor_idx}:([-+]?\d*\.?\d+)", raw)
                    mr = re.search(fr"Pressure_rate_kPa_{sensor_idx}:([-+]?\d*\.?\d+)", raw)

                    t_unix = time.time()
                    elapsed = t_unix - t0
                    pressure_kpa = float(m.group(2))
                    pressure_psi = pressure_kpa / 6.8947572932
                    weight_g = float(mw.group(1)) if mw else None
                    pressure_rate_kpa = float(mr.group(1)) if mr else None

                    # log to CSV (include weight if present)
                    if weight_g is None or pressure_rate_kpa is None:
                        w.writerow([f"{t_unix:.6f}", f"{sensor_idx}", f"{pressure_kpa:.3f}", f"{pressure_psi:.3f}", "", ""])
                    else:
                        w.writerow([f"{t_unix:.6f}", f"{sensor_idx}", f"{pressure_kpa:.3f}", f"{pressure_psi:.3f}", f"{weight_g:.3f}", f"{pressure_rate_kpa:.3f}"])
                    f.flush()

                    # sample-rate calculation
                    if last_unix_with_idx.get(sensor_idx, None) is not None:
                        dt = t_unix - last_unix_with_idx[sensor_idx]
                        if dt > 0:
                            dt_samples_with_idx[sensor_idx] = dt_samples_with_idx.get(sensor_idx, deque(maxlen=50)).copy()
                            dt_samples_with_idx[sensor_idx].append(dt)
                    last_unix_with_idx[sensor_idx] = t_unix

                    if dt_samples_with_idx[sensor_idx]:
                        mean_dt = sum(dt_samples_with_idx[sensor_idx]) / len(dt_samples_with_idx[sensor_idx])
                        rate_hz = 1.0 / mean_dt if mean_dt > 0 else float('nan')
                    else:
                        rate_hz = float('nan')

                    # append to buffers
                    times.append(elapsed)
                    pressures.append(pressure_kpa)

                    # update plot (if enabled)
                    if not no_plot:
                        line.set_data(times, pressures)
                        if times:
                            ax.set_xlim(max(0.0, times[0]), times[-1] + 0.5)
                            # autoscale y with small margin
                            pmin, pmax = min(pressures), max(pressures)
                            if pmin == pmax:
                                ax.set_ylim(pmin - 0.5, pmax + 0.5)
                            else:
                                margin = 0.1 * (pmax - pmin)
                                ax.set_ylim(pmin - margin, pmax + margin)

                        # set title with sample rate
                        ax.set_title(f"Live Pressure — {rate_hz:.2f} Hz")

                        fig.canvas.draw()
                        fig.canvas.flush_events()

                    # always print to console (include weight if available)
                    if weight_g is None or pressure_rate_kpa is None:
                        print(f"{t_unix:.3f}, {sensor_idx}, {pressure_kpa:.3f} kPa, {pressure_psi:.3f} psi, {rate_hz:.2f} Hz  ->  {out_path}")
                    else:
                        print(f"{t_unix:.3f}, {sensor_idx}, {pressure_kpa:.3f} kPa, {pressure_psi:.3f} psi, {weight_g:.3f} g, {pressure_rate_kpa:.3f} kPa/s, {rate_hz:.2f} Hz  ->  {out_path}")

        except KeyboardInterrupt:
            print("Interrupted by user, closing...")
        finally:
            ser.close()
            if not no_plot:
                plt.ioff()
                if times:
                    # show final plot non-interactively
                    fig.tight_layout()
                    plt.show()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Read MPRLS pressure, log to CSV, optional live plot")
    parser.add_argument("--plot", action="store_true", help="Enable live plotting (default: no plot)")
    args = parser.parse_args()
    main(no_plot=not args.plot)
