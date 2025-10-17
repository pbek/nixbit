# Nixbit

A small KDE Plasma application built with QtQuick that displays a terminal running `git status`.

## Features

- QtQuick-based user interface
- Displays output of `git status` command in a terminal-like view
- Refresh button to re-run the command
- Built with CMake and KDE Frameworks

## Building

### Using devenv (Recommended for Development)

1. Enter the development environment:

```bash
devenv shell
```

2. Build and run:

```bash
just build
just run
```

### Using Nix Package (Recommended for Deployment)

Build and run the Nix package:

```bash
# Build the package
just nix-build

# Run the built package
just nix-run

# Or with nix-build directly
nix-build -E 'with import <nixpkgs> { }; callPackage ./package.nix { }'
./result/bin/nixbit
```

### Using Nix Flakes

If you have flakes enabled:

```bash
# Build the package
nix build

# Run directly
nix run

# Enter development shell
nix develop

# Build using just
just nix-build-flake
```

### Manual build

Requirements:

- CMake 3.16+
- Qt 6.5+
- KDE Frameworks 6.0+
- GCC or Clang with C++17 support

Build commands:

```bash
mkdir build
cd build
cmake .. -GNinja
cmake --build .
./bin/nixbit
```

## Just Commands

### Development

- `just build` - Configure and build the application
- `just run` - Build and run the application
- `just clean` - Clean build artifacts
- `just rebuild` - Clean and rebuild from scratch

### Nix Package

- `just nix-build` - Build the Nix package
- `just nix-run` - Build and run the Nix package
- `just nix-install` - Install to user profile
- `just nix-result` - Show the build result

## Development

The project uses:

- **CMake** for build configuration
- **Qt Quick** for the UI
- **KDE Frameworks** for KDE integration
- **devenv** for development environment management
- **just** for convenient build commands
- **Nix** for reproducible packaging

## License

This project is provided as-is for demonstration purposes.
cmake_minimum_required(VERSION 3.16)

project(nixbit VERSION 1.0)

set(QT_MIN_VERSION "5.15.0")
set(KF5_MIN_VERSION "5.82.0")

find_package(ECM ${KF5_MIN_VERSION} REQUIRED NO_MODULE)
set(CMAKE_MODULE_PATH ${ECM_MODULE_PATH})

include(KDEInstallDirs)
include(KDECMakeSettings)
include(KDECompilerSettings NO_POLICY_SCOPE)
include(FeatureSummary)

find_package(Qt5 ${QT_MIN_VERSION} CONFIG REQUIRED COMPONENTS
Core
Quick
Widgets
)

find_package(KF5 ${KF5_MIN_VERSION} REQUIRED COMPONENTS
CoreAddons
I18n
)

add_executable(nixbit
src/main.cpp
)

target_link_libraries(nixbit
Qt5::Core
Qt5::Quick
Qt5::Widgets
KF5::CoreAddons
KF5::I18n
)

install(TARGETS nixbit ${KDE_INSTALL_TARGETS_DEFAULT_ARGS})
install(FILES src/main.qml DESTINATION ${KDE_INSTALL_DATADIR}/nixbit)

feature_summary(WHAT ALL INCLUDE_QUIET_PACKAGES FATAL_ON_MISSING_REQUIRED_PACKAGES)
