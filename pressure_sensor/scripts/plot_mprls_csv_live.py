# plot_mprls_csv_live.py
import argparse

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np

def main():
    parser = argparse.ArgumentParser(
        prog="plot_mprls_csv_live.py",
        description="Live-plot MPRLS CSV and show interactive cursor",
    )
    parser.add_argument("csv", help="path to mprls CSV file")
    parser.add_argument(
        "--start-offset",
        type=float,
        default=0.0,
        help="seconds to skip from start; x-axis will start at 0 after this offset",
    )
    # parser.add_argument(
    #     "--enable-estimator-pipeline-simulator",
    #     action="store_true",
    #     help="enable estimator pipeline simulator",
    # )

    args = parser.parse_args()
    csv_path = args.csv
    start_offset = float(args.start_offset)
    enable_estimator_pipeline_simulator = bool(args.enable_estimator_pipeline_simulator)

    number_of_plots = 2 if enable_estimator_pipeline_simulator else 3

    # if enable_estimator_pipeline_simulator:
    #     from estimator_pipeline_simulator import EstimatorPipelineSimulator
    #     simulator = EstimatorPipelineSimulator(window_size=5)

    fig, axes = plt.subplots(number_of_plots, 1, sharex=True, figsize=(8, 6))
    (line,) = axes[0].plot([], [], linewidth=2)
    (rate_line,) = axes[1].plot([], [], linewidth=2, color="tab:orange")

    # arrays holding current plotted data (updated in update())
    xdata = np.array([])
    ydata = np.array([])
    y2data = np.array([])

    # interactive vertical cursors and annotations for both subplots
    vline1 = axes[0].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    vline2 = axes[1].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    annot1 = axes[0].annotate(
        "",
        xy=(0, 0),
        xytext=(15, 15),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    )
    annot2 = axes[1].annotate(
        "",
        xy=(0, 0),
        xytext=(15, -30),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    )

    fig.suptitle("MPRLS Pressure and Pressure Rate vs Time")

    # axes[0].set_title("MPRLS Pressure vs Time")
    axes[0].set_xlabel("Time since start (s)")
    axes[0].set_ylabel("Pressure (kPa)")
    axes[0].grid(True)
    # axes[1].set_title("MPRLS Pressure Rate vs Time")
    axes[1].set_xlabel("Time since start (s)")
    axes[1].set_ylabel("Pressure Rate (kPa/s)")
    axes[1].grid(True)

    def update(_frame):
        try:
            df = pd.read_csv(csv_path)  # common way to load CSV into DataFrame
        except Exception:
            return line,

        if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
            return line,

        nonlocal xdata, ydata, y2data

        t0 = float(df["t_unix_s"].iloc[0])
        xraw = df["t_unix_s"].astype(float) - t0
        yraw = df["pressure_kpa"].astype(float)

        # apply start offset: keep samples with time >= start_offset
        mask = xraw >= start_offset
        if not mask.any():
            # no data after offset; clear plots
            xdata = np.array([])
            ydata = np.array([])
            y2data = np.array([])
            line.set_data(xdata, ydata)
            rate_line.set_data(xdata, y2data)
            return line,

        x = xraw[mask] - start_offset
        y = yraw[mask]

        # update stored arrays and line data
        xdata = x.to_numpy()
        ydata = y.to_numpy()
        # pressure rate column (if present)
        if "pressure_rate_kpa_s" in df.columns:
            y2 = df["pressure_rate_kpa_s"].astype(float)[mask]
            y2data = y2.to_numpy()
        else:
            y2data = np.full_like(xdata, np.nan, dtype=float)

        line.set_data(xdata, ydata)
        rate_line.set_data(xdata, y2data)
        axes[0].relim()
        axes[0].autoscale_view()
        axes[1].relim()
        axes[1].autoscale_view()
        return line,

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
