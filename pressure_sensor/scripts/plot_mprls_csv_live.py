# plot_mprls_csv_live.py
import argparse
from pathlib import Path

import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import numpy as np
from estimator_pipeline_simulator import EstimatorPipelineSimulator, GRAMS_TO_NEWTONS, NEWTONS_TO_GRAMS

# if False, will use pressure_rate_kpa_s column if present;
# if True, will calculate rate from pressure_kpa column regardless of presence of pressure_rate_kpa_s column
USE_RAW_PRESSURE_TO_CALCULATE_RATE = True
# FORCE_CORRECTION_FACTOR = 0.2 # empirical correction factor for 12mmx15mm
FORCE_CORRECTION_FACTOR = 1.0 # empirical correction factor for 15mmx16mm

def main():
    parser = argparse.ArgumentParser(
        prog="plot_mprls_csv_live.py",
        description="Live-plot MPRLS CSV and show interactive cursor",
    )
    parser.add_argument("csv", help="path to mprls CSV file")
    parser.add_argument("--prefix", help="optional prefix to filter CSV files in same directory (e.g. to only plot sensor 0 data from extract_individual_sensor_data.py output)")
    parser.add_argument(
        "--start-offset",
        type=float,
        default=0.0,
        help="seconds to skip from start; x-axis will start at 0 after this offset",
    )
    parser.add_argument(
        "--enable-estimator-pipeline-simulator",
        action="store_true",
        help="enable estimator pipeline simulator",
    )

    args = parser.parse_args()
    csv_path = Path(args.csv)
    prefix = args.prefix if args.prefix else None
    start_offset = float(args.start_offset)
    enable_estimator_pipeline_simulator = bool(args.enable_estimator_pipeline_simulator)

    fig, axes = plt.subplots(3, 1, sharex=True, figsize=(8, 6))
    (line,) = axes[0].plot([], [], linewidth=2, label="Pressure (kPa)")
    (rate_line,) = axes[1].plot([], [], linewidth=2, color="tab:orange", label="Pressure Rate (kPa/s)")
    (force_line,) = axes[2].plot([], [], linewidth=2, color="tab:green", label="Force (N)")
    if enable_estimator_pipeline_simulator:
        (force_sim_line,) = axes[2].plot([], [], linewidth=2, color="tab:purple", linestyle="--", label="Estimator Sim Force (N)")

    # arrays holding current plotted data (updated in update())
    xdata = np.array([])
    ydata = np.array([])
    y2data = np.array([])
    y3data = np.array([])  # for real-time estimator output
    y3sim_data = np.array([])  # for simulated estimator output

    # interactive vertical cursors and annotations for both subplots
    vline1 = axes[0].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    vline2 = axes[1].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
    vline3 = axes[2].axvline(x=0, color="k", linestyle="--", alpha=0.7, visible=False)
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
    annot3 = axes[2].annotate(
        "",
        xy=(0, 0),
        xytext=(15, 15),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    )
    annot4 = axes[2].annotate(
        "",
        xy=(0, 0),
        xytext=(15, -30),
        textcoords="offset points",
        bbox=dict(boxstyle="round", fc="w"),
        arrowprops=dict(arrowstyle="->"),
        visible=False,
    ) if enable_estimator_pipeline_simulator else None

    fig.suptitle(f"MPRLS Pressure, Rate, and Force vs Time:\n{prefix}\n{csv_path.stem}" if prefix else f"MPRLS Pressure, Rate, and Force vs Time:\n{csv_path.stem}")

    # show legends for each subplot
    axes[0].legend(loc="upper right")
    axes[1].legend(loc="upper right")
    axes[2].legend(loc="upper right")

    # axes[0].set_title("MPRLS Pressure vs Time")
    # axes[0].set_xlabel("Time since start (s)")
    axes[0].set_ylabel("Pressure (kPa)")
    axes[0].grid(True)
    # axes[1].set_title("MPRLS Pressure Rate vs Time")
    # axes[1].set_xlabel("Time since start (s)")
    axes[1].set_ylabel("Pressure Rate (kPa/s)")
    axes[1].grid(True)
    # axes[1].set_title("MPRLS Pressure Rate vs Time")
    # axes[1].set_xlabel("Time since start (s)")
    axes[1].set_ylabel("Pressure Rate (kPa/s)")
    axes[1].grid(True)
    # axes[2].set_title("MPRLS Force vs Time")
    axes[2].set_xlabel("Time since start (s)")
    axes[2].set_ylabel("Force (N)")
    axes[2].grid(True)

    def update(_frame):
        try:
            df = pd.read_csv(csv_path.absolute())  # common way to load CSV into DataFrame
        except Exception:
            return tuple([line, rate_line, force_line] + ([force_sim_line] if enable_estimator_pipeline_simulator else []))

        if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
            return tuple([line, rate_line, force_line] + ([force_sim_line] if enable_estimator_pipeline_simulator else []))

        nonlocal xdata, ydata, y2data, y3data, y3sim_data

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
            y3data = np.array([])
            y3sim_data = np.array([])
            line.set_data(xdata, ydata)
            rate_line.set_data(xdata, y2data)
            force_line.set_data(xdata, y3data)
            if enable_estimator_pipeline_simulator:
                force_sim_line.set_data(xdata, y3sim_data)
            return tuple([line, rate_line, force_line] + ([force_sim_line] if enable_estimator_pipeline_simulator else []))

        x = xraw[mask] - start_offset
        y = yraw[mask]

        # update stored arrays and line data
        xdata = x.to_numpy()
        ydata = y.to_numpy()
        # pressure rate column (if present)
        if "pressure_rate_kpa_s" in df.columns:
            if USE_RAW_PRESSURE_TO_CALCULATE_RATE:
                # calculate rate from pressure column regardless of presence of pressure_rate_kpa_s column
                y2data = np.gradient(ydata, xdata)  # simple numerical differentiation
            else:
                y2 = df["pressure_rate_kpa_s"].astype(float)[mask]
                y2data = y2.to_numpy()
        else:
            y2data = np.full(len(xdata), np.nan, dtype=float)

        # grams to Newtons for force plot
        if "weight_g" in df.columns:
            y3 = df["weight_g"].astype(float)[mask] * GRAMS_TO_NEWTONS * FORCE_CORRECTION_FACTOR
            y3data = y3.to_numpy()
        else:
            y3data = np.full(len(xdata), np.nan, dtype=float)

        if enable_estimator_pipeline_simulator:
            # TODO: process simulated estimator output using interpolated pressure and pressure rate at cursor x position; this is a simple simulation and can be replaced with a more complex model if needed
            simulator = EstimatorPipelineSimulator()
            y3sim_data = np.array([simulator.process(pressure, rate) for pressure, rate in zip(ydata, y2data)]) * FORCE_CORRECTION_FACTOR

        # ensure arrays match lengths to avoid broadcasting errors
        if ydata.shape != xdata.shape:
            ydata = np.full(len(xdata), np.nan, dtype=float)
        if y2data.shape != xdata.shape:
            y2data = np.full(len(xdata), np.nan, dtype=float)
        if y3data.shape != xdata.shape:
            y3data = np.full(len(xdata), np.nan, dtype=float)
        if enable_estimator_pipeline_simulator and y3sim_data.shape != xdata.shape:
            y3sim_data = np.full(len(xdata), np.nan, dtype=float)

        line.set_data(xdata, ydata)
        rate_line.set_data(xdata, y2data)
        force_line.set_data(xdata, y3data)
        if enable_estimator_pipeline_simulator:
            force_sim_line.set_data(xdata, y3sim_data)
        axes[0].relim()
        axes[0].autoscale_view()
        axes[1].relim()
        axes[1].autoscale_view()
        axes[2].relim()
        axes[2].autoscale_view()
        return tuple([line, rate_line, force_line] + ([force_sim_line] if enable_estimator_pipeline_simulator else []))

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
            vline3.set_visible(False)
            annot1.set_visible(False)
            annot2.set_visible(False)
            annot3.set_visible(False)
            if enable_estimator_pipeline_simulator:
                annot4.set_visible(False)
            fig.canvas.draw_idle()
            return

        y = float(np.interp(x, xdata, ydata))
        y_rate = float(np.interp(x, xdata, y2data)) if y2data.size and not np.isnan(y2data).all() else float("nan")
        y_force = float(np.interp(x, xdata, y3data)) if y3data.size and not np.isnan(y3data).all() else float("nan")
        y_force_sim = float(np.interp(x, xdata, y3sim_data)) if enable_estimator_pipeline_simulator and y3sim_data.size and not np.isnan(y3sim_data).all() else float("nan")

        vline1.set_xdata([x, x])
        vline2.set_xdata([x, x])
        vline3.set_xdata([x, x])
        vline1.set_visible(True)
        vline2.set_visible(True)
        vline3.set_visible(True)

        annot1.xy = (x, y)
        annot1.set_text(f"t={x:.2f}s\n{y:.3f} kPa")
        annot1.set_visible(True)

        annot2.xy = (x, y_rate)
        annot2.set_text(f"t={x:.2f}s\n{y_rate:.3f} kPa/s")
        annot2.set_visible(True)

        annot3.xy = (x, y_force)
        annot3.set_text(f"t={x:.2f}s\n{y_force:.3f} N")
        annot3.set_visible(True)

        if enable_estimator_pipeline_simulator:
            annot4.xy = (x, y_force_sim)
            annot4.set_text(f"t={x:.2f}s\n{y_force_sim:.3f} N")
            annot4.set_visible(True)

        fig.canvas.draw_idle()

    def _on_leave(event):
        vline1.set_visible(False)
        vline2.set_visible(False)
        vline3.set_visible(False)
        annot1.set_visible(False)
        annot2.set_visible(False)
        annot3.set_visible(False)
        if enable_estimator_pipeline_simulator:
            annot4.set_visible(False)
        fig.canvas.draw_idle()

    # connect event handlers
    fig.canvas.mpl_connect("motion_notify_event", _on_mouse_move)
    fig.canvas.mpl_connect("axes_leave_event", _on_leave)

    ani = FuncAnimation(fig, update, interval=20)  # periodically refresh plot
    plt.show()

if __name__ == "__main__":
    main()
