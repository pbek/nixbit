# Build and run commands for Nixbit

# Default recipe - show available commands
default:
    @just --list

# Configure the project with CMake
configure:
    mkdir -p build
    cd build && cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the application
build: configure
    cmake --build build

# Clean build artifacts
clean:
    rm -rf build

# Run the application
run: build
    #!/usr/bin/env bash
    export QML_IMPORT_PATH="${QML_IMPORT_PATH}"
    export QT_PLUGIN_PATH="${QT_PLUGIN_PATH}"
    export QT_QPA_PLATFORM="${QT_QPA_PLATFORM:-xcb}"
    exec ./build/bin/nixbit

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
    nix-build -E 'with import <nixpkgs> { }; callPackage ./package.nix { }'

# Build the Nix package using flakes (if available)
nix-build-flake:
    nix build .#nixbit

# Install the Nix package to user profile
nix-install:
    nix-env -f . -i nixbit

# Run the Nix-built package
nix-run:
    ./result/bin/nixbit

# Show the build result
nix-result:
    @ls -la result/bin/
