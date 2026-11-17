# Build and run commands for Nixbit

import ".shared/common.just"
import ".shared/cpp.just"

# Default recipe - show available commands
default:
    @just --list

# Variables

transferDir := `if [ -d "$HOME/NextcloudPrivate/Transfer" ]; then echo "$HOME/NextcloudPrivate/Transfer"; else echo "$HOME/Nextcloud/Transfer"; fi`

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
    ./build/nixbit --debug

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

# Build and run the Nix package
nix-build-run: nix-build nix-run

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
    exec result/bin/nixbit --debug

# Capture all demo scenes on the current desktop (or pass --scenes)
[group('screenshots')]
screenshots *args:
    python3 ./scripts/screenshots.py {{ args }}

# Publish reviewed captures as lossless WebP images
[group('screenshots')]
screenshots-publish capture_dir="":
    python3 ./scripts/screenshots.py --publish {{ capture_dir }}

# Capture every scene in a fresh, isolated NixOS Plasma VM
[group('screenshots')]
screenshots-vm *args:
    #!/usr/bin/env bash
    set -euo pipefail
    if [[ ! -r /dev/kvm || ! -w /dev/kvm ]]; then
        echo "Screenshot VMs require Linux with access to /dev/kvm." >&2
        exit 1
    fi
    system=$(nix eval --impure --raw --expr builtins.currentSystem)
    stamp=$(date +%Y%m%d-%H%M%S-%N)
    result=$(NIXBIT_SCREENSHOT_RUN="$stamp" nix build 'path:{{ justfile_directory() }}#checks.'"$system"'.screenshots' --impure --no-link --print-out-paths -L {{ args }})
    output="build-screenshots/$stamp-vm"
    mkdir -p build-screenshots
    cp -R "$result/screenshots" "$output"
    chmod -R u+w "$output"
    echo "VM screenshots: $output"

# Open the screenshot VM driver without replacing the application result link
[group('screenshots')]
screenshots-vm-interactive:
    #!/usr/bin/env bash
    set -euo pipefail
    system=$(nix eval --impure --raw --expr builtins.currentSystem)
    driver=$(nix build 'path:{{ justfile_directory() }}#checks.'"$system"'.screenshots.driverInteractive' --no-link --print-out-paths -L)
    echo "Run start_all() to boot Plasma, or test_script() to capture all scenes."
    "$driver/bin/nixos-test-driver"

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
