#!/usr/bin/env python3
"""Join openRBRVR perf log runs with the NGPCarMenu RaceStat database.

    python perflog_join.py "<RBR>/Plugins/openRBRVR/perf/runs.csv" "<RBR>/Plugins/NGPCarMenu/RaceStat/raceStatDB.sqlite3" > joined.csv

The database is copied to a temporary file and opened read-only, the original is never touched.
A run is matched to the RaceStat row with the same stage id (MapID) and car slot (CarSlot) whose
local end time (RaceDate + RaceDateTime) is closest to the end of the run, within WINDOW_S seconds.
Runs without a match (e.g. retired before the first split, which RaceStat does not record) are kept
with empty RaceStat columns.
"""

import csv
import datetime as dt
import shutil
import sqlite3
import sys
import tempfile
from pathlib import Path

WINDOW_S = 15

SKY_CLOUD = {0: "clear", 1: "part", 2: "light", 3: "heavy"}
SKY = {0: "crisp", 1: "hazy", 2: "norain", 3: "lightrain", 4: "heavyrain", 5: "nosnow",
       6: "lightsnow", 7: "heavysnow", 8: "lightfog", 9: "heavyfog"}
WET = {0: "dry", 1: "damp", 2: "wet"}
TIME_OF_DAY = {0: "morning", 1: "noon", 2: "evening"}

PERF_COLS = [
    "timestamp", "stage_id", "stage_name", "car_id", "end_reason", "driving_s", "frames",
    "render_context", "res_left", "supersampling", "msaa", "peripheral_msaa", "quad_view", "multiview",
    "budget_ms",
    "cpu_p50_ms", "cpu_p99_ms", "cpu_max_ms",
    "gpu_p50_ms", "gpu_p99_ms", "gpu_max_ms",
    "interval_p50_ms", "interval_p99_ms", "interval_max_ms",
    "over_budget_pct", "over_70pct_pct", "late_pct",
]


def end_local(row):
    """Local end time of the run as datetime. Older records only have the UTC start and the duration."""
    if row.get("end_local_date") and row.get("end_local_time"):
        return dt.datetime.strptime(row["end_local_date"] + row["end_local_time"].zfill(6), "%Y%m%d%H%M%S")
    start = dt.datetime.strptime(row["timestamp"], "%Y-%m-%dT%H:%M:%SZ").replace(tzinfo=dt.timezone.utc)
    end = start + dt.timedelta(seconds=float(row["duration_s"]))
    return end.astimezone().replace(tzinfo=None)


def load_results(db_path):
    with tempfile.TemporaryDirectory() as tmp:
        copy = Path(tmp) / "raceStatDB.sqlite3"
        shutil.copyfile(db_path, copy)
        con = sqlite3.connect(f"file:{copy}?mode=ro", uri=True)
        con.text_factory = lambda b: b.decode("utf-8", "replace")
        con.row_factory = sqlite3.Row
        rows = con.execute(
            "SELECT F.RaceDate, F.RaceDateTime, M.MapID, F.CarSlot, C.ModelName, F.FinishTime, "
            "F.FalseStartPenaltyTime + F.CutPenaltyTime + F.OtherPenaltyTime AS Penalty, "
            "F.TimeOfDay, F.WeatherType, F.SkyCloudType, F.SkyType, F.SurfaceWetness, F.TyreType, F.TyreSubType "
            "FROM F_RallyResult F JOIN D_Map M USING (MapKey) JOIN D_Car C USING (CarKey)"
        ).fetchall()
        con.close()
    return [dict(r) for r in rows]


def match(results, row, end):
    best, best_delta = None, WINDOW_S + 1
    for r in results:
        if r["MapID"] != int(row["stage_id"]) or r["CarSlot"] != int(row["car_id"]):
            continue
        t = dt.datetime.strptime(f"{r['RaceDate']}{str(r['RaceDateTime']).zfill(6)}", "%Y%m%d%H%M%S")
        delta = abs((t - end).total_seconds())
        if delta < best_delta:
            best, best_delta = r, delta
    return best


def main(runs_csv, db_path):
    results = load_results(db_path)
    out = csv.writer(sys.stdout, lineterminator="\n")
    extra = ["car_model", "finish_time_s", "penalty_s", "time_of_day", "weather", "clouds", "sky", "wetness", "tyre"]
    out.writerow(PERF_COLS + extra)
    with open(runs_csv, newline="", encoding="utf-8") as f:
        for row in csv.DictReader(f):
            r = match(results, row, end_local(row))
            if r:
                finish = f"{r['FinishTime']:.3f}" if r["FinishTime"] is not None else ""
                joined = [r["ModelName"], finish, f"{r['Penalty']:.1f}", TIME_OF_DAY.get(r["TimeOfDay"], "?"),
                          {"G": "good", "B": "bad"}.get(r["WeatherType"], "?"), SKY_CLOUD.get(r["SkyCloudType"], "?"),
                          SKY.get(r["SkyType"], "?"), WET.get(r["SurfaceWetness"], "?"), r["TyreType"] + r["TyreSubType"]]
            else:
                joined = [""] * len(extra)
            out.writerow([row.get(c, "") for c in PERF_COLS] + joined)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
