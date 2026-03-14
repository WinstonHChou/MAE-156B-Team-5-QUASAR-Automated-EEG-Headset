"""Read MPRLS pressure values from serial, log to CSV and plot live.

This replaces the previous simple logger with a live matplotlib plot while
continuing to append timestamped values to a CSV file.
"""

import re
import time
import csv
from datetime import datetime
from collections import deque, defaultdict

import serial
from serial.tools import list_ports
from packets import PacketID, ControlFlags, RequestType, ControlPacket, SensorPacket, SerialBridge
import matplotlib.pyplot as plt
import argparse

BAUD = 921600


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

    bridge = SerialBridge(port, BAUD)

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
    dt_samples_with_idx = defaultdict(lambda: deque(maxlen=50))

    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(["t_unix_s", "sensor_idx", "pressure_kpa", "pressure_psi", "weight_g", "pressure_rate_kpa_s"])  # header

        try:
            while True:
                pkt = bridge.receive()
                if not isinstance(pkt, SensorPacket):
                    # keep loop idle-friendly; if plotting is enabled, pump GUI events
                    if not no_plot:
                        plt.pause(0.001)
                    else:
                        time.sleep(0.001)
                    continue

                t_unix = time.time()
                elapsed = t_unix - t0
                sensor_idx = pkt.sensor_idx
                pressure_kpa = pkt.sensor_pressure_kPa
                pressure_psi = pressure_kpa / 6.8947572932
                weight_g = pkt.sensor_force_g
                pressure_rate_kpa_s = pkt.sensor_pressure_rate_kPa_s

                w.writerow([f"{t_unix:.6f}", f"{sensor_idx}", f"{pressure_kpa:.3f}", f"{pressure_psi:.3f}", f"{weight_g:.3f}", f"{pressure_rate_kpa_s:.3f}"])
                f.flush()

                # sample-rate calculation
                if last_unix_with_idx.get(sensor_idx, None) is not None:
                    dt = t_unix - last_unix_with_idx[sensor_idx]
                    if dt > 0:
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

                # always print to console
                print(f"{t_unix:.3f}, {sensor_idx}, {pressure_kpa:.3f} kPa, {pressure_psi:.3f} psi, {weight_g:.3f} g, {pressure_rate_kpa_s:.3f} kPa/s, {rate_hz:.2f} Hz  ->  {out_path}")

        except KeyboardInterrupt:
            try:
                bridge.getTransfer().close()
                print("Interrupted by user, closing...")
            except:
                pass

        except:
            import traceback
            traceback.print_exc()

            try:
                bridge.getTransfer().close()
            except:
                pass

        finally:
            bridge.getTransfer().close()
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
