# read_mprls_pressure_to_csv.py
import re
import time
import csv
from datetime import datetime

import serial
from serial.tools import list_ports

BAUD = 115200
KEY = "Pressure (kPa):"

def autodetect_port():
    ports = list(list_ports.comports())  # common pyserial way to enumerate ports [web:10]
    if not ports:
        raise RuntimeError("No serial ports found.")
    if len(ports) == 1:
        return ports[0].device

    print("Available ports:")
    for i, p in enumerate(ports):
        print(f"  [{i}] {p.device}  {p.description}")
    idx = int(input("Select port index: "))
    return ports[idx].device

def main():
    port = autodetect_port()
    ts = datetime.now().strftime("%Y%m%d_%H%M%S")
    out_path = f"mprls_log_{ts}.csv"

    ser = serial.Serial(port, BAUD, timeout=0.5)
    time.sleep(2.0)  # Arduino often resets when the port opens

    with open(out_path, "w", newline="") as f:  # newline="" is recommended for csv module [web:16]
        w = csv.writer(f)
        w.writerow(["t_unix_s", "pressure_kpa"])  # header row via writerow() [web:16]

        try:
            while True:
                line = ser.readline().decode(errors="ignore").strip()
                if not line:
                    continue

                if line.startswith(KEY):
                    m = re.search(r"([-+]?\d*\.?\d+)", line)
                    if not m:
                        continue

                    t = time.time()
                    pressure_kpa = float(m.group(1))

                    w.writerow([f"{t:.6f}", f"{pressure_kpa:.3f}"])
                    f.flush()  # ensures data is written out during long runs (useful for logging) [web:25]

                    print(f"{t:.3f}, {pressure_kpa:.3f} kPa  ->  {out_path}")
        except KeyboardInterrupt:
            pass
        finally:
            ser.close()

if __name__ == "__main__":
    main()
