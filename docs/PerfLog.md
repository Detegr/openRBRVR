# Performance log

An optional logger that writes one summary record per stage run: frame times (CPU, GPU, pacing),
the render settings that were in use, and how many frames missed the HMD frame budget. It is meant
for tuning per-stage graphics settings (`[gfx.*]` render contexts) from real data.

It is **off by default** and has no cost when off.

## Enabling

In `openRBRVR.toml`:

```toml
perfLog = true        # one summary record per run
perfLogDetail = false # additionally a per-second CSV for each run
```

Both can also be toggled in the plugin menu ("Performance log", "Performance log detail"), saved with
"Save the current config". The performance log does not depend on the debug overlay (`debug`), the
overlay can stay off.

## What is recorded

A run starts at the first driving frame of a stage and ends when the stage finishes or is retired, is
restarted, the player goes back to the menu, another stage is loaded, or the game exits (`end_reason`).
Pausing and replays are part of the run but their time is not counted in the statistics (`paused_s`).
Runs shorter than 30 frames are discarded.

The statistics cover **all driving frames, including the start countdown**. The plugin API callbacks
for the stage start are not called by every launcher (they are not called with RSF) and openRBRVR has no
other reliable source for the race clock. Use `worst_frames` / the detail file to see where in the
run a spike happened.

Timings are the same values the debug overlay uses:

* `cpu`: time from the start of the frame (render hook) to just before `Present`
* `gpu`: GPU render time from the timestamp query (OpenXR) or the compositor frame timing (OpenVR).
  With OpenXR the query pair only completes about every second frame, so `gpu_samples` is roughly
  half of `frames`
* `frame`: `max(cpu, gpu)`, the value the "FPS only" overlay colors are based on (the last GPU value is
  carried over for frames without a new measurement)
* `interval`: time between the starts of two consecutive frames (real pacing)
* `wait`: OpenXR: time spent in `xrWaitFrame`. OpenVR: compositor wait for present
* `present`: OpenVR only: time spent in the compositor present call

The frame budget is the HMD display period reported by the runtime (`predictedDisplayPeriod` for OpenXR,
display frequency for OpenVR). If the runtime does not report one, the highest FPS seen so far is used,
like the overlay does (`budget_source`).

`over_budget_*` counts frames where `frame` > budget, `over_70pct_*` frames where `frame` > 70% of the
budget (the yellow/red thresholds of the FPS overlay). `late_*` counts frames the runtime delivered late:
OpenXR: predicted display time advanced more than 1.5 display periods since the previous frame, OpenVR:
the compositor reported a dropped or mispresented frame. `skipped_frames`: OpenXR `shouldRender` was
false (some runtimes always report that, so it can be meaningless).

Percentiles come from fixed size histograms (0.05 ms bins up to 102.4 ms), the maximum and mean are exact.

## Files

Everything is written to `Plugins/openRBRVR/perf/`, only when a run ends.

* `runs.csv` and `runs.jsonl`: the same records, one line per run. A header line is written when the
  CSV is created. If the columns change in a future version, move the old file away.
* `stage<id>-<YYYYMMDD>-<HHMMSS>.csv` (UTC start of the run): per-second rows with `perfLogDetail`:
  `t_s,frames,cpu_mean_ms,cpu_max_ms,gpu_mean_ms,gpu_max_ms,gpu_samples`

### Columns of `runs.csv`

| Column | Meaning |
| --- | --- |
| `timestamp`, `end_timestamp` | UTC start / end of the run, ISO 8601 |
| `end_local_date`, `end_local_time` | local end time as `YYYYMMDD`, `HHMMSS` (see "Joining" below) |
| `version`, `commit`, `dxvk`, `xr_runtime`, `gpu` | openRBRVR version and build commit, DXVK version, OpenXR runtime name and version (or SteamVR version), GPU name |
| `stage_id`, `stage_name`, `stage_length_km`, `stage_surface` | stage id, and name / length / surface from `Maps/Tracks.ini` (empty if not found) |
| `car_id` | car slot of the run, same as `CarSlot` in the NGPCarMenu RaceStat database |
| `btb` | 1 for a BTB stage |
| `end_reason` | `finish` (game reached its end-of-stage state, also when retiring), `restart`, `menu`, `loading`, `stage_change`, `quit`, `disabled` |
| `duration_s`, `paused_s`, `driving_s` | wall-clock duration, time not driving (pause, replay...), driving time |
| `frames` | number of driving frames |
| `render_context` | name of the `[gfx.*]` context |
| `res_left`, `res_right`, `res_focus_left`, `res_focus_right` | render resolution per eye (focus views only with quad view rendering) |
| `supersampling`, `msaa`, `peripheral_msaa` | settings of the render context. MSAA as number of samples |
| `quad_view`, `multiview` | 1 if in use |
| `qvf_user_settings` | with quad view rendering: your overrides in `%LOCALAPPDATA%\Quad-Views-Foveated\settings.cfg`, as `section.key=value;...` |
| `runtime` | `openxr` or `openvr` |
| `hmd_refresh_hz`, `budget_ms`, `budget_source` | frame budget, see above |
| `cpu_*`, `gpu_*`, `frame_*`, `interval_*`, `wait_*`, `present_*` | `_mean_ms`, `_p50_ms`, `_p90_ms`, `_p99_ms`, `_max_ms` |
| `gpu_samples` | number of GPU measurements |
| `over_budget_frames/_pct`, `over_70pct_frames/_pct`, `late_frames/_pct`, `skipped_frames` | see above |
| `worst_frames`, `worst_intervals` | the 10 slowest frames / longest frame intervals as `t=ms;...`, `t` in seconds on the driving clock |
| `detail_file` | name of the per-second file, if any |

### Sample

Abridged `runs.jsonl` record (OpenXR, quad views, 90 Hz HMD):

```json
{"timestamp":"2026-11-27T09:41:06Z","version":"2.2.4","stage_id":600,"car_id":5,"end_reason":"finish","duration_s":258.9,"paused_s":64.1,"frames":16639,"render_context":"default","res_left":"3406x3252","res_focus_left":"2384x2276","supersampling":1.00,"msaa":2,"quad_view":1,"multiview":1,"runtime":"openxr","hmd_refresh_hz":90.0,"budget_ms":11.11,"budget_source":"hmd","cpu_mean_ms":1.95,"cpu_p50_ms":1.84,"cpu_p90_ms":2.34,"cpu_p99_ms":3.29,"cpu_max_ms":20.39,"gpu_mean_ms":1.76,"gpu_p50_ms":1.66,"gpu_p90_ms":2.14,"gpu_p99_ms":2.99,"gpu_max_ms":20.09,"over_budget_frames":18,"over_budget_pct":0.11,"over_70pct_frames":49,"over_70pct_pct":0.29}
```

## Joining with other data

`tools/perflog_join.py` joins `runs.csv` with the NGPCarMenu RaceStat database
(`Plugins/NGPCarMenu/RaceStat/raceStatDB.sqlite3`, read-only on a temporary copy). That adds finish time,
penalties, time of day, weather and surface wetness to each run:

```
python tools/perflog_join.py Plugins/openRBRVR/perf/runs.csv Plugins/NGPCarMenu/RaceStat/raceStatDB.sqlite3 > joined.csv
```

RaceStat stores the local end time of a run (`RaceDate`, `RaceDateTime`), the stage id (`MapID`) and the car
slot (`CarSlot`). A run matches the row with the same stage and slot whose end time is closest, within 15 s.
Runs that were retired before the first split are not stored by RaceStat and stay unmatched.

## Cost

* Disabled: one boolean check per frame.
* Enabled: a few clock reads, counter and histogram updates per frame. No allocation, locking, string
  formatting or file I/O in the frame loop. Memory is bounded: about 50 KB of histograms, plus with
  `perfLogDetail` one row of 28 bytes per second of driving (capped at 4 hours).
* On OpenXR the GPU timestamp queries the debug overlay uses are also issued while `perfLog` is on.
* At the end of a run the records are written synchronously, together with a parse of `Maps/Tracks.ini`
  for the stage name. This happens once per run, outside of driving.
