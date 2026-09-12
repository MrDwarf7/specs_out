<p align="center">
  <strong>specs_out</strong> -- A compelling tagline
  <br>
  <a href="https://github.com/mrdwarf7/specs_out/actions/workflows/ci.yml"><img src="https://github.com/mrdwarf7/specs_out/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
  <a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-blue" alt="license"></a>
  <a href="https://github.com/mrdwarf7/specs_out/releases"><img src="https://img.shields.io/github/v/release/mrdwarf7/specs_out" alt="release"></a>
</p>

## specs_out

A longer 2-3 sentence description of what this project does and who it's for.

```bash
# Quick example
specs_out input.txt -o output.txt
```

## Features

- **Feature 1** -- Description
- **Feature 2** -- Description
- **Feature 3** -- Description
- **Cross-platform** -- Linux, macOS, Windows (via GCC/MSVC)

## Installation

### From Source

| Dependency | Minimum Version |
|------------|-----------------|
| GCC or Clang | C23 support (GCC 14+, Clang 18+) |
| make | Any POSIX make |
| cppcheck | Optional, for static analysis |

```bash
git clone https://github.com/mrdwarf7/specs_out.git
cd specs_out
make build_release
sudo cp build_rel/specs_out /usr/local/bin/
```

### One-liner (Linux)

```bash
curl -fsSL https://github.com/mrdwarf7/specs_out/raw/main/build/install.sh | sh
```

Installs to `/usr/local/bin` (or `~/.local/bin` if not writable).

### Release Archives

Download from [Releases](https://github.com/mrdwarf7/specs_out/releases/latest).

Each archive contains:

```
specs_out-<os>-<arch>.tar.gz
  specs_out
```

### Supported Targets

| OS | Arch | Notes |
|----|------|-------|
| Linux | x86_64 | Primary target |
| Linux | aarch64 | Cross-compile |
| macOS | x86_64, arm64 | Via Clang |
| Windows | x86_64 | Via MSVC/MinGW |

## Usage

```bash
specs_out [OPTIONS] <INPUT>
```

### Examples

```bash
# Basic usage
specs_out input.txt

# With output file
specs_out input.txt -o output.txt

# Dry run (show what would happen)
specs_out --dry-run input.txt
```

### Flags

| Flag | Short | Description |
|------|-------|-------------|
| `--help` | `-h` | Show help |
| `--version` | `-V` | Print version |
| `--output` | `-o` | Output file path |
| `--dry-run` | | Show actions without executing |
| `--quiet` | `-q` | Suppress non-error output |
| `--verbose` | `-v` | Verbose output (repeat for more) |

## Build

```bash
# Debug build (default) -- outputs to build/
make

# Release build -- outputs to build_rel/
make build_release

# Run all tests
make test

# Clean build + test
make cl && make t

# Static analysis (cppcheck)
make check

# Format code with clang-format
make format

# All: debug + release + test
make a
```

## Dependencies

| Tool | Required | Install |
|------|----------|---------|
| GCC 14+ or Clang 18+ | Yes | `pacman -S gcc` / `apt install gcc-14` |
| make | Yes | `pacman -S make` / `apt install build-essential` |
| cppcheck | No | `pacman -S cppcheck` / `apt install cppcheck` |
| clang-format | No | `pacman -S clang` / `apt install clang-format` |

## Configuration

Environment variables:
- `SPECS_OUT_LOG=debug` -- Enable debug logging

## How It Works

1. **Step 1** -- Description
2. **Step 2** -- Description
3. **Step 3** -- Description

## Links

- [Changelog](CHANGELOG.md)
- [Issues](https://github.com/mrdwarf7/specs_out/issues)

## License

MIT -- see [LICENSE](LICENSE).
