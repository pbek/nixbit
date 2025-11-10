# Build and run commands for Nixbit

import ".shared/common.just"
import ".shared/cpp.just"

# Default recipe - show available commands
default:
    @just --list

# Variables

transferDir := `if [ -d "$HOME/NextcloudPrivate/Transfer" ]; then echo "$HOME/NextcloudPrivate/Transfer"; else echo "$HOME/Nextcloud/Transfer"; fi`

# Clear QML cache
clear-qml-cache:
    @echo "Clearing QML cache..."
    @rm -rf ~/.cache/pbek/nixbit/qmlcache 2>/dev/null || true
    @rm -rf ~/.cache/pbek/nixbit-debug/qmlcache 2>/dev/null || true
    @echo "QML cache cleared"

# Configure the project with CMake
configure:
    mkdir -p build
    cd build && cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the application
build:
    mkdir -p build
    cd build && cmake .. && cmake --build .

# Clean build artifacts
clean: clear-qml-cache
    rm -rf build

# Run the application
run: build
    QML_DISABLE_DISK_CACHE=1 ./build/nixbit --debug

# Rebuild from scratch
rebuild: clean build

# Install the application (may require sudo)
install: build
    cmake --install build

# Run tests (if any)
test: build
    cd build && ctest --output-on-failure

# Build the Nix package
nix-build: clear-qml-cache
    nix-build -E '(import <nixpkgs> {}).callPackage ./package.nix {}'

# Build the Nix package using flakes (if available)
flake-build: clear-qml-cache
    nix build .#nixbit

# Install the Nix package to user profile
nix-install:
    nix-env -f . -i nixbit

# Run the Nix-built package (requires X11/Wayland display)
nix-run:
    #!/usr/bin/env bash
    set -euo pipefail
    echo "Running nixbit from nix build result..."
    if [ ! -e result/bin/nixbit ]; then
        echo "Error: result/bin/nixbit not found. Run 'just nix-build' first."
        exit 1
    fi
    if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
        echo "Warning: No DISPLAY or WAYLAND_DISPLAY environment variable set."
        echo "The application requires a graphical display to run."
        echo "If you're running this on a server, you may need to use Xvfb or run it on a system with a display."
    fi
    exec result/bin/nixbit --debug

# Show the build result
nix-result:
    @ls -la result/bin/

# Verify the executable was built correctly
nix-check:
    #!/usr/bin/env bash
    if [ ! -e result/bin/nixbit ]; then
        echo "❌ result/bin/nixbit not found. Run 'just nix-build' first."
        exit 1
    fi
    echo "✓ Executable exists: result/bin/nixbit"
    file result/bin/nixbit
    echo ""
    echo "✓ Checking library dependencies..."
    if ldd result/bin/nixbit | grep -i "not found" > /dev/null 2>&1; then
        echo "❌ Missing libraries detected:"
        ldd result/bin/nixbit | grep "not found"
        exit 1
    else
        echo "✓ All libraries found"
    fi
    echo ""
    echo "✓ Build appears successful!"

# Apply a git patch to the project
[group('patches')]
git-apply-patch:
    git apply {{ transferDir }}/nixbit.patch

# Create git patches for the project
[group('patches')]
git-create-patch:
    @echo "transferDir: {{ transferDir }}"
    git diff --no-ext-diff --staged --binary > {{ transferDir }}/nixbit.patch
    ls -l1t {{ transferDir }} | head -2
