#!/usr/bin/env bash
# dev-start.sh -- quick startup script for specs_out dev environment
#
# Usage:
#   ./scripts/dev-start.sh          # build + run tests
#   ./scripts/dev-start.sh build    # build only (debug + release)
#   ./scripts/dev-start.sh test     # run all tests
#   ./scripts dev-start.sh check    # static analysis
#   ./scripts dev-start.sh clean    # clean build artifacts
#
# For Docker-based dev:
#   docker compose run --rm build
#   docker compose run --rm shell

set -euo pipefail

BUILD_DIR="build"
PROJECT_NAME="specs_out"

build_debug() {
    echo "[1/3] Building debug..."
    make build_debug
}

build_release() {
    echo "[2/3] Building release..."
    make build_release
}

run_tests() {
    echo "[3/3] Running tests..."
    make test
}

run_check() {
    echo "Running static analysis..."
    make check
}

clean() {
    echo "Cleaning..."
    make clean
}

case "${1:-all}" in
    all)
        clean
        build_debug
        build_release
        run_tests
        ;;
    build)
        build_debug
        build_release
        ;;
    test)
        run_tests
        ;;
    check)
        run_check
        ;;
    clean)
        clean
        ;;
    *)
        echo "Usage: $0 {all|build|test|check|clean}"
        exit 1
        ;;
esac
