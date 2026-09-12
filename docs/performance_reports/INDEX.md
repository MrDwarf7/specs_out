# Performance reports

This directory holds hyperfine end-to-end timings for `specs_out`. Every
cell ran 100 iterations through Makefile `bench-*` recipes (no raw `gcc`).
Cold cells use zero warmup and `sync` before each run. Hot cells use 20
warmup runs. Page-cache drop was not available (this process cannot write
`/proc/sys/vm/drop_caches`).

Re-run the full matrix with `make perf`. Override counts with `RUNS=100
WARMUP_HOT=20 bash scripts/perf-hyperfine.sh`.

## Host used for these runs

These timings were captured on 12 September 2026. The machine has a
faster NVMe drive, but this tree and `docs/performance_reports/` live
on a SATA SSD. Do not treat later NVMe numbers as the same baseline.

Facts below come from a live `specs_out --dry-run` plus `lsblk` /
`findmnt` for the filesystem that actually holds this repo. Other
disks that `specs_out` prints (NVMe Windows layout, extra HDD/SSD
mounts) are omitted on purpose.

- **CPU:** AMD Ryzen 7 7700X — 8 cores / 8 threads (SMT off), boosting
  to ~5.6GHz
- **RAM:** 32 GiB DDR5 (~19 GiB available at capture time)
- **GPU:** NVIDIA GeForce RTX 4080 — 16 GB VRAM
- **Motherboard:** ASRock X670E Steel Legend
- **Disk under test:** Samsung SSD 870 EVO 250GB, 232.9 GB, SATA
  (`sda`, `ROTA=0`). Repo and reports are on `/dev/sda2` btrfs
  subvolume `/@home` mounted at `/home` (233G, about 79% used).
  `/boot` is `sda1` (512 MB vfat) on the same drive.
- **Swap:** 4 GB zram (active)
- **OS:** Arch Linux, kernel 7.2.3-arch1-3
- **Hyperfine:** 1.20.0

This is a 2.5-inch SATA SSD, not an M.2 NVMe. Sequential and random
access are slower than the 931.5 GB NVMe also present in this box
(`nvme0n1`, not the filesystem these benches wrote).

## Matrix

The grid is build × workload × cache. Debug and release binaries both
cover help, preview (`--dry-run`), serial preview (`SPECS_OUT_JOBS=1`),
and write (`-t`). The test runner covers the full suite plus parse,
format, CLI, and capture.

| Build   | Workload       | Cold                              | Hot                              |
| ------- | -------------- | --------------------------------- | -------------------------------- |
| debug   | help           | `raw/debug_help_cold`             | `raw/debug_help_hot`             |
| debug   | preview        | `raw/debug_preview_cold`          | `raw/debug_preview_hot`          |
| debug   | preview serial | `raw/debug_preview_serial_cold`   | `raw/debug_preview_serial_hot`   |
| debug   | write          | `raw/debug_write_cold`            | `raw/debug_write_hot`            |
| release | help           | `raw/release_help_cold`           | `raw/release_help_hot`           |
| release | preview        | `raw/release_preview_cold`        | `raw/release_preview_hot`        |
| release | preview serial | `raw/release_preview_serial_cold` | `raw/release_preview_serial_hot` |
| release | write          | `raw/release_write_cold`          | `raw/release_write_hot`          |
| tests   | all            | `raw/tests_all_cold`              | `raw/tests_all_hot`              |
| tests   | parse          | `raw/tests_parse_cold`            | `raw/tests_parse_hot`            |
| tests   | format         | `raw/tests_format_cold`           | `raw/tests_format_hot`           |
| tests   | CLI            | `raw/tests_cli_cold`              | `raw/tests_cli_hot`              |
| tests   | capture        | `raw/tests_capture_cold`          | `raw/tests_capture_hot`          |

Each cell has four artifacts: `.log` (hyperfine stdout), `.json`,
`.md`, and `.csv` under `raw/`. Write-to-disk cells used
`scratch/<cell>.specs` so `$HOME/.specs` was never touched.

## Summary (mean of 100)

Times are wall-clock seconds from hyperfine. Preview and write are
dominated by spawning `lsblk` / `lscpu` / `nvidia-smi`, so debug vs
release is close. Serial gather (`SPECS_OUT_JOBS=1`) is slower than
the default eight-worker gather.

| cell                        | mean (s) | stddev (s) |  min (s) |  max (s) |
| --------------------------- | -------: | ---------: | -------: | -------: |
| debug_help_cold             | 0.016284 |   0.002406 | 0.013746 | 0.037631 |
| debug_help_hot              | 0.015989 |   0.000967 | 0.013971 | 0.019969 |
| debug_preview_cold          | 0.167377 |   0.021416 | 0.136810 | 0.205671 |
| debug_preview_hot           | 0.160087 |   0.019968 | 0.136070 | 0.192481 |
| debug_preview_serial_cold   | 0.204820 |   0.022448 | 0.170774 | 0.241845 |
| debug_preview_serial_hot    | 0.200870 |   0.022162 | 0.170743 | 0.235468 |
| debug_write_cold            | 0.165420 |   0.021562 | 0.136892 | 0.202478 |
| debug_write_hot             | 0.159521 |   0.020802 | 0.134910 | 0.193369 |
| release_help_cold           | 0.015837 |   0.000699 | 0.014214 | 0.017945 |
| release_help_hot            | 0.016164 |   0.001645 | 0.013489 | 0.024481 |
| release_preview_cold        | 0.164605 |   0.020567 | 0.136696 | 0.201940 |
| release_preview_hot         | 0.166312 |   0.020955 | 0.134884 | 0.198196 |
| release_preview_serial_cold | 0.206645 |   0.021514 | 0.171578 | 0.236163 |
| release_preview_serial_hot  | 0.202624 |   0.022285 | 0.175432 | 0.245995 |
| release_write_cold          | 0.165515 |   0.021509 | 0.136462 | 0.203828 |
| release_write_hot           | 0.166346 |   0.020890 | 0.138532 | 0.198919 |
| tests_all_cold              | 0.024250 |   0.003503 | 0.020507 | 0.046828 |
| tests_all_hot               | 0.023171 |   0.002161 | 0.018968 | 0.029953 |
| tests_capture_cold          | 0.023773 |   0.002082 | 0.019836 | 0.029702 |
| tests_capture_hot           | 0.023308 |   0.002121 | 0.019168 | 0.031108 |
| tests_cli_cold              | 0.015569 |   0.001030 | 0.013347 | 0.018208 |
| tests_cli_hot               | 0.015781 |   0.000889 | 0.013997 | 0.019283 |
| tests_format_cold           | 0.015683 |   0.000896 | 0.013607 | 0.018142 |
| tests_format_hot            | 0.015479 |   0.000942 | 0.013115 | 0.018048 |
| tests_parse_cold            | 0.015740 |   0.000778 | 0.014104 | 0.017773 |
| tests_parse_hot             | 0.015352 |   0.000869 | 0.013280 | 0.017872 |

## Next steps

Run `make perf` after a gather or capture change to refresh `raw/` and
`summary_table.md`.
