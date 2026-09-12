<!-- rumdl-disable MD033 -->
<h1 align="center">specs_out</h1>

<div align="center">

**A compact Linux hardware report, printed to your terminal.**

[![C23](https://img.shields.io/badge/language-C23-00599C?style=for-the-badge&logo=c&logoColor=white)](https://github.com/topics/c23)
[![Linux](https://img.shields.io/badge/platform-Linux-FCC624?style=for-the-badge&logo=linux&logoColor=black)](https://github.com/topics/linux)
[![CI](https://img.shields.io/github/actions/workflow/status/MrDwarf7/specs_out/ci.yml?branch=main&style=for-the-badge)](https://github.com/MrDwarf7/specs_out/actions/workflows/ci.yml)
[![Hardware info](https://img.shields.io/badge/topic-hardware--info-555555?style=for-the-badge)](https://github.com/topics/hardware-info)

</div>
<!-- rumdl-enable MD033 -->

Gather CPU, memory, GPU, motherboard, storage, swap, network, and OS details in
one bullet-formatted report. Print it to stdout by default, or save it with
`-o PATH`.

[Quick start](#quick-start) | [Features](#features) | [Usage](#usage) | [Dependencies](#dependencies) | [Build and tests](#build-and-tests)

## Quick start

Build from source with a C23 compiler (GCC 14+ or Clang 18+), `make`, and
`clang-format`. The Makefile defaults to GCC; use `make br CC=clang` for Clang.
Linux x86_64 is the platform built and timed in this repository.

```sh
git clone https://github.com/MrDwarf7/specs_out.git
cd specs_out
make br
./build_rel/specs_out
```

To install the binary system-wide:

```sh
sudo install -m 755 build_rel/specs_out /usr/local/bin/specs_out
```

Example output (hardware and available memory vary):

```text
— CPU: AMD Ryzen 7 7700X — 8 cores / 8 threads (SMT off), boosting to ~5.6GHz
— RAM: 32 GiB DDR5 (~19 GiB available)
— GPU: NVIDIA GeForce RTX 4080 — 16 GB VRAM
— Motherboard: ASRock X670E Steel Legend
- Boot drive: 232.9 GB SSD (sda) — 512 MB /boot, 232.4 GB /
— Swap: 4 GB zram (active)
— Network: Wi-Fi (wlan0, UP), 2x Ethernet (enp5s0/enp8s0, DOWN)
— OS: Arch Linux, kernel 7.2.3-arch1-3
```

## Features

- One report covering hardware and OS details, with no file written by default.
- The complete report is buffered in memory before printing. Stdout write or
  flush failures return a nonzero exit status.
- Explicit file output with overwrite confirmation and non-interactive protection.
- Eight gather workers by default, with a serial mode for troubleshooting.

## Usage

```sh
specs_out [options] [PATH]
```

A positional `PATH` is the same as `-o PATH`.

| Flag            | Short | Meaning                                 |
| --------------- | ----- | --------------------------------------- |
| `--truncate`    | `-t`  | Replace an existing file without asking |
| `--yes`         | `-y`  | Same as `--truncate`                    |
| `--dry-run`     | `-n`  | Print the document, write nothing       |
| `--output PATH` | `-o`  | Write to PATH instead of stdout         |
| `--help`        | `-h`  | Show help                               |

If the destination exists and you did not pass `-t`/`-y`, interactive stdin
prompts before replacing it. Non-interactive stdin refuses to clobber the file.

```sh
# Print to the terminal
specs_out

# Save to the traditional location
specs_out -o "$HOME/.specs"

# Replace an existing file without prompting
specs_out -t -o /tmp/my.specs

# Preview a file write without changing the file
specs_out --dry-run -o /tmp/my.specs
```

## Dependencies

Runtime probes use `lscpu`, `lsblk`, `findmnt`, `ip`, `uname`, `lspci`,
`nvidia-smi`, `inxi`, and `du` when present, alongside sysfs files. Optional
GPU/memory probes such as `nvidia-smi` and `inxi` have fallbacks; missing
`lscpu`, `lsblk`, or `uname` is fatal.

## Configuration

Set `SPECS_OUT_JOBS=1` to gather serially instead of using eight workers:

```sh
SPECS_OUT_JOBS=1 specs_out
```

`PATH` controls command lookup. `HOME` is not required; there is no implicit
output destination or configuration file.

## How it works

Independent command dumps are captured with `posix_spawn` into bump arenas.
After workers join, parsers turn `lsblk`, `ip`, and `lscpu` output into tables.
Formatters assemble the report in order: CPU, RAM, GPU, motherboard, disks,
swap, network, and OS.

## Build and tests

```sh
make b          # Debug build -> build/
make br         # Release build -> build_rel/
make t          # Unit tests
make cl && make t
make check      # cppcheck
make a          # Debug + release + tests
make perf       # hyperfine matrix -> docs/performance_reports/
```

`make r` / `make rr` print to stdout. Use `make run-dry OUT=/path` to preview a
file write or `make run-write OUT=/path` to write it.

`make t` runs parse, format, CLI, and capture suites against shipped functions
using fixture dumps rather than live hardware. Capture tests spawn `uname` on
eight threads to check `pipe2(O_CLOEXEC)` and the PATH cache.

## Links

- [Performance reports](docs/performance_reports/INDEX.md): timings and the
  machine used for those measurements.
- [Issues](https://github.com/MrDwarf7/specs_out/issues)
- [CI](https://github.com/MrDwarf7/specs_out/actions/workflows/ci.yml)

## License

No license file is included yet. Treat this repository as source-available;
it does not currently grant an open-source license.
