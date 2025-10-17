# Build and run commands for Nixbit

# Default recipe - show available commands
default:
    @just --list

# Configure the project with CMake
configure:
    mkdir -p build
    cd build && cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the application
build:
    mkdir -p build
    cd build && cmake .. && cmake --build .

# Clean build artifacts
clean:
    rm -rf build

# Run the application
run: build
    ./build/bin/nixbit

# Rebuild from scratch
rebuild: clean build

# Install the application (may require sudo)
install: build
    cmake --install build

# Run tests (if any)
test: build
    cd build && ctest --output-on-failure

# Build the Nix package
nix-build:
    nix-build -E '(import <nixpkgs> {}).callPackage ./package.nix {}'

# Build the Nix package using flakes (if available)
flake-build:
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
    exec result/bin/nixbit

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
