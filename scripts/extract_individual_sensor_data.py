# extract_individual_sensor_data.py
import sys
import time
from pathlib import Path

import pandas as pd

def main():
    if len(sys.argv) < 2:
        print("Usage: python extract_individual_sensor_data.py path/to/mprls_log_YYYYMMDD_HHMMSS.csv")
        raise SystemExit(2)

    csv_path = Path(sys.argv[1])

    df = pd.read_csv(csv_path.absolute())
    if df.empty or "t_unix_s" not in df.columns or "pressure_kpa" not in df.columns:
        print("CSV file is missing required columns.")
        raise SystemExit(1)
    
    df["sensor_idx"] = df["sensor_idx"].astype(int)
    unique_sensor_indices = set(df["sensor_idx"].unique())  # get unique sensor indices

    for idx in sorted(unique_sensor_indices):
        sensor_df = df[df["sensor_idx"] == idx]
        out_path = f"{csv_path.parent}/{csv_path.stem}_sensor_{idx}.csv"
        sensor_df.to_csv(out_path, index=False)
        print(f"Extracted data for sensor {idx} to {out_path}")

if __name__ == "__main__":
    main()
