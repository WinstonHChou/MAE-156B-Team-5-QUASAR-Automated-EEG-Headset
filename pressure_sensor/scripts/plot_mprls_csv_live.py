# plot_mprls_csv_live.py
import sys
import time

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation  # standard live-update approach [web:36]

def main():
    if len(sys.argv) < 2:
        print("Usage: python plot_mprls_csv_live.py path/to/mprls_log_YYYYMMDD_HHMMSS.csv")
        raise SystemExit(2)

    csv_path = sys.argv[1]

    fig, ax = plt.subplots()
    (line,) = ax.plot([], [], linewidth=2)

    ax.set_title("MPRLS Pressure vs Time")
    ax.set_xlabel("Time since start (s)")
    ax.set_ylabel("Pressure (kPa)")
    ax.grid(True)

    def update(_frame):
        try:
            df = pd.read_csv(csv_path)  # common way to load CSV into DataFrame [web:40]
        except Exception:
            return line,

        if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
            return line,

        t0 = float(df["t_unix_s"].iloc[0])
        x = df["t_unix_s"].astype(float) - t0
        y = df["pressure_kpa"].astype(float)

        line.set_data(x.to_numpy(), y.to_numpy())
        ax.relim()
        ax.autoscale_view()
        return line,

    ani = FuncAnimation(fig, update, interval=200)  # periodically refresh plot [web:36]
    plt.show()

if __name__ == "__main__":
    main()
