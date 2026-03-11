# plot_pressure_drifting.py
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
import numpy as np

ENABLE_SIM = False  # Set to True to enable air diffusion model simulation

def air_diffusion_model(dt, P_k, P_amb, k):
    """
    Simple exponential decay model for pressure drifting due to air diffusion.
    P_k1 = P_k - k * (P_k - P_amb) * dt
    where:
        P_amb = ambient pressure
        k = diffusion rate constant (higher k means faster drifting)
    """
    return P_k - k * (P_k - P_amb) * dt


def main():
    parser = argparse.ArgumentParser(prog="plot_pressure_drifting.py", description="Plot Pressure CSV")
    parser.add_argument("csv", help="path to pressure CSV file")
    parser.add_argument("--second-csv", help="optional second CSV file to plot on same axes (e.g. for ambient pressure data)")
    parser.add_argument("--prefix", help="optional prefix to filter CSV files in same directory (e.g. to only plot sensor 0 data from extract_individual_sensor_data.py output)")
    parser.add_argument(
        "--start-offset",
        type=float,
        default=0.0,
        help="seconds to skip from start; x-axis will start at 0 after this offset",
    )

    args = parser.parse_args()
    csv_path = Path(args.csv)
    second_csv_path = Path(args.second_csv) if args.second_csv else None
    prefix = args.prefix if args.prefix else None
    start_offset = float(args.start_offset)

    fig, ax = plt.subplots(figsize=(8, 6))
    (line,) = ax.plot([], [], linewidth=2, label="Pressure (kPa)")
    line2 = None
    line_est = None
    line_sim = None
    if second_csv_path:
        (line2,) = ax.plot([], [], linewidth=2, color="tab:orange", label="Ambient Pressure (kPa)")
        (line_est,) = ax.plot([], [], linewidth=2, color="tab:purple", linestyle=":", label="Estimated Pressure (kPa)")
        if ENABLE_SIM:
            (line_sim,) = ax.plot([], [], linewidth=2, color="tab:green", linestyle="--", label="Air Diffusion Model (kPa)")

    fig.suptitle(f"Pressure vs Time:\n{prefix}\n{csv_path.stem}" if prefix else f"Pressure vs Time:\n{csv_path.stem}")

    ax.legend(loc="upper right")
    ax.set_xlabel("Time since start (s)")
    ax.set_ylabel("Pressure (kPa)")
    ax.grid(True)

    try:
        df = pd.read_csv(csv_path.absolute())
        df2 = pd.read_csv(second_csv_path.absolute()) if second_csv_path else None
    except Exception as exc:
        raise SystemExit(f"Failed to read CSV file(s): {exc}")

    if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
        raise SystemExit("Primary CSV must contain non-empty 't_unix_s' and 'pressure_kpa' columns.")

    t0 = float(df["t_unix_s"].iloc[0])
    xraw = df["t_unix_s"].astype(float) - t0
    yraw = df["pressure_kpa"].astype(float)
    x2raw = df2["t_unix_s"].astype(float) - t0 if df2 is not None and not df2.empty and "t_unix_s" in df2.columns else None
    y2raw = df2["pressure_kpa"].astype(float) if df2 is not None and not df2.empty and "pressure_kpa" in df2.columns else None

    # apply start offset: keep samples with time >= start_offset
    mask = xraw >= start_offset
    mask2 = x2raw >= start_offset if x2raw is not None else None
    if not mask.any():
        raise SystemExit("No samples remain after applying --start-offset.")

    x = xraw[mask] - start_offset
    y = yraw[mask]
    x2 = x2raw[mask2] - start_offset if x2raw is not None and mask2 is not None else None
    y2 = y2raw[mask2] if y2raw is not None and mask2 is not None else None

    xdata = x.to_numpy()
    ydata = y.to_numpy()
    x2data = x2.to_numpy() if x2 is not None else np.array([])
    y2data = y2.to_numpy() if y2 is not None else np.array([])
    y2data = np.interp(xdata, x2data, y2data) if x2data.size and y2data.size else np.full_like(xdata, np.nan)
    ydata_est = []
    ydata_sim = []
    if xdata.size and ydata.size and y2data.size:
        k = 0.000125  # diffusion rate constant, adjust as needed
        dt = np.diff(xdata, prepend=xdata[0])  # time differences between samples
        drift = np.cumsum(k * dt * (ydata - y2data))
        ydata_est = ydata + drift  # estimated pressure without drifting
        P_k = ydata[1000]
        for i in range(len(xdata)):
            P_amb = y2data[i]
            P_k = air_diffusion_model(dt[i], P_k, P_amb, k)
            ydata_sim.append(P_k)
    else:
        ydata_sim = np.full_like(xdata, np.nan)

    # ensure arrays match lengths to avoid broadcasting errors
    if ydata.shape != xdata.shape:
        ydata = np.full(len(xdata), np.nan, dtype=float)
    if y2data.shape != xdata.shape:
        y2data = np.full(len(xdata), np.nan, dtype=float)

    line.set_data(xdata, ydata)
    if line2 is not None:
        line2.set_data(xdata, y2data)
    if line_est is not None:
        line_est.set_data(xdata, ydata_est)
    if line_sim is not None and ENABLE_SIM:
        line_sim.set_data(xdata, ydata_sim)
    ax.relim()
    ax.autoscale_view()
    plt.show()

if __name__ == "__main__":
    main()
