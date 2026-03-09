# plot_pressure.py
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np


def main():
    parser = argparse.ArgumentParser(
        prog="plot_pressure.py",
        description="Live-plot Pressure CSV and show interactive cursor",
    )
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

    fig, axes = plt.subplots(2, 1, sharex=True, figsize=(8, 6))
    (line,) = axes[0].plot([], [], linewidth=2, label="Pressure (kPa)")
    (line2,) = axes[0].plot([], [], linewidth=2, color="tab:orange", label="Ambient Pressure (kPa)")

    # arrays holding current plotted data (updated in update())
    xdata = np.array([])
    ydata = np.array([])
    y2data = np.array([])

    # interactive vertical cursors and annotations for both subplots
    vline1 = axes[0].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    vline2 = axes[0].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    annot1 = axes[0].annotate(
        "",
        xy=(0, 0),
        xytext=(15, 15),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    )
    annot2 = axes[0].annotate(
        "",
        xy=(0, 0),
        xytext=(15, -30),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    )

    fig.suptitle(f"Pressure vs Time:\n{prefix}\n{csv_path.stem}" if prefix else f"Pressure vs Time:\n{csv_path.stem}")

    # show legends for each subplot
    axes[0].legend(loc="upper right")

    # axes[0].set_title("MPRLS Pressure vs Time")
    # axes[0].set_xlabel("Time since start (s)")
    axes[0].set_ylabel("Pressure (kPa)")
    axes[0].grid(True)

    def update(_frame):
        try:
            df = pd.read_csv(csv_path.absolute())  # common way to load CSV into DataFrame
            df2 = pd.read_csv(second_csv_path.absolute()) if second_csv_path else None  # load second CSV if provided
        except Exception:
            return tuple([line] + ([line2] if second_csv_path else []))

        if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
            return tuple([line] + ([line2] if second_csv_path else []))

        nonlocal xdata, ydata, y2data

        t0 = float(df["t_unix_s"].iloc[0])
        xraw = df["t_unix_s"].astype(float) - t0
        yraw = df["pressure_kpa"].astype(float)
        x2raw = df2["t_unix_s"].astype(float) - t0 if df2 is not None and not df2.empty and "t_unix_s" in df2.columns else None
        y2raw = df2["pressure_kpa"].astype(float) if df2 is not None and not df2.empty and "pressure_kpa" in df2.columns else None

        # apply start offset: keep samples with time >= start_offset
        mask = xraw >= start_offset
        mask2 = x2raw >= start_offset if x2raw is not None else None
        if not mask.any():
            # no data after offset; clear plots
            xdata = np.array([])
            ydata = np.array([])
            y2data = np.array([])
            line.set_data(xdata, ydata)
            line2.set_data(xdata, y2data)
            return tuple([line] + ([line2] if second_csv_path else []))

        x = xraw[mask] - start_offset
        y = yraw[mask]
        x2 = x2raw[mask2] - start_offset if x2raw is not None and mask2 is not None else None
        y2 = y2raw[mask2] if y2raw is not None and mask2 is not None else None

        # update stored arrays and line data
        xdata = x.to_numpy()
        ydata = y.to_numpy()
        x2data = x2.to_numpy() if x2 is not None else np.array([])
        y2data = y2.to_numpy() if y2 is not None else np.array([])
        y2data = np.interp(xdata, x2data, y2data) if x2data.size and y2data.size else np.full_like(xdata, np.nan)

        # ensure arrays match lengths to avoid broadcasting errors
        if ydata.shape != xdata.shape:
            ydata = np.full(len(xdata), np.nan, dtype=float)
        if y2data.shape != xdata.shape:
            y2data = np.full(len(xdata), np.nan, dtype=float)

        line.set_data(xdata, ydata)
        line2.set_data(xdata, y2data)
        axes[0].relim()
        axes[0].autoscale_view()
        return tuple([line] + ([line2] if second_csv_path else []))

    def _on_mouse_move(event):
        # show vertical cursor and interpolated values when mouse over axes
        if event.inaxes not in axes:
            return
        if xdata.size == 0:
            return
        x = event.xdata
        if x is None:
            return
        x = float(x)
        if x < xdata[0] or x > xdata[-1]:
            vline1.set_visible(False)
            vline2.set_visible(False)
            annot1.set_visible(False)
            annot2.set_visible(False)
            fig.canvas.draw_idle()
            return

        y = float(np.interp(x, xdata, ydata))
        y_rate = float(np.interp(x, xdata, y2data)) if y2data.size and not np.isnan(y2data).all() else float("nan")

        vline1.set_xdata([x, x])
        vline2.set_xdata([x, x])
        vline1.set_visible(True)
        vline2.set_visible(True)

        annot1.xy = (x, y)
        annot1.set_text(f"t={x:.2f}s\n{y:.3f} kPa")
        annot1.set_visible(True)

        annot2.xy = (x, y_rate)
        annot2.set_text(f"t={x:.2f}s\n{y_rate:.3f} kPa/s")
        annot2.set_visible(True)

        fig.canvas.draw_idle()

    def _on_leave(event):
        vline1.set_visible(False)
        vline2.set_visible(False)
        annot1.set_visible(False)
        annot2.set_visible(False)
        fig.canvas.draw_idle()

    # connect event handlers
    fig.canvas.mpl_connect("motion_notify_event", _on_mouse_move)
    fig.canvas.mpl_connect("axes_leave_event", _on_leave)

    ani = FuncAnimation(fig, update, interval=20)  # periodically refresh plot
    plt.show()

if __name__ == "__main__":
    main()
