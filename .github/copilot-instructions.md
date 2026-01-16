# GitHub Copilot Instructions for Nixbit

## Project Overview

Nixbit is a **GUI application for updating NixOS systems** from a Nix Flakes Git repository. It's built using:

- **Language**: C++ (C++17)
- **Build System**: CMake + Ninja
- **GUI Framework**: Qt6 (QML/QtQuick)
- **KDE Frameworks**: KF6 (CoreAddons, I18n)
- **Package Manager**: Nix with Flakes
- **Task Runner**: Just (justfile)
- **Version Control**: libgit2 for Git operations

## Project Structure

```
src/                    # Source code (C++ and QML)
  *.cpp, *.h           # C++ implementation files
  *.qml                # QML UI files
  main.cpp             # Application entry point
  version.h.in         # Version template
build/                 # CMake build directory (generated)
flake.nix              # Nix flake definition
package.nix            # Nix package definition
CMakeLists.txt         # CMake configuration
justfile               # Task runner commands
```

## Key Managers/Components

The application uses several manager classes:

- `GitManager` - Git repository operations using libgit2
- `ProcessManager` - External process execution (nix commands)
- `LogManager` - System log viewing
- `GenerationManager` - NixOS generation management
- `SettingsManager` - Application settings
- `SystemMonitor` - System resource monitoring
- `TrayIconManager` - System tray integration
- `SystemResumeDetector` - Detect system resume from sleep

## Building and Running

### Build with Nix (Recommended)

Use **Just** commands for all build operations:

```bash
# Build the Nix package
just nix-build

# Run the Nix-built package
just nix-run

# Build using flakes
just flake-build

# Verify the build
just nix-check
```

### Build with CMake (Development)

```bash
# Configure the project
just configure

# Build the application
just build

# Run the application
just run

# Clean build artifacts
just clean

# Rebuild from scratch
just rebuild
```

### Testing

```bash
# Run tests
just test
```

## Important Build Notes

1. **Always use `just nix-build`** for building the Nix package - this ensures proper Nix environment
2. **Use `just build`** for quick development iterations with CMake
3. The application requires **X11 or Wayland** display to run (GUI application)
4. Build output goes to `build/` directory for CMake builds
5. Nix builds create a `result/` symlink to the Nix store

## Dependencies

### Runtime Dependencies

- Qt6 (Core, Gui, Qml, Quick, Widgets, DBus, Network)
- KDE Frameworks 6 (CoreAddons, I18n)
- libgit2

### Build Dependencies

- CMake 3.20+
- Ninja
- Qt6 development packages
- KDE Extra CMake Modules
- pkg-config

## Code Style Guidelines

### C++ Code

- **Standard**: C++17
- **MOC**: Qt's Meta-Object Compiler enabled (CMAKE_AUTOMOC)
- **Include Guards**: Use `#pragma once`
- **Naming**:
  - Classes: PascalCase (e.g., `GitManager`)
  - Files: lowercase (e.g., `gitmanager.cpp`, `gitmanager.h`)
  - Methods: camelCase
- **Headers**: Separate `.h` and `.cpp` files

### QML Code

- Files in `src/*.qml`
- Dialog files follow pattern: `*Dialog.qml`
- Use Qt Quick Controls 2
- JavaScript utilities in `Utils.js`

### CMake

- Minimum version: 3.20
- Use `qt_add_executable` for Qt6
- Auto-generate resources with AUTORCC

## Version Management

Version is defined in `CMakeLists.txt`:

```cmake
project(nixbit VERSION 0.7.1 LANGUAGES CXX)
```

The version is also extracted by `package.nix` automatically from CMakeLists.txt.

## Nix Flake Structure

The project includes:

- **Package**: `nixbit` application
- **NixOS Module**: `nixosModules.nixbit` for system-wide configuration
- **Development Shell**: Available via `nix develop`

## Common Tasks

### Adding New C++ Source Files

1. Create `.h` and `.cpp` files in `src/`
2. Add to `CMakeLists.txt` in the `qt_add_executable` section
3. If using Qt signals/slots, ensure the header has `Q_OBJECT` macro

### Adding New QML Files

1. Create `.qml` file in `src/`
2. Add to `CMakeLists.txt` in the QML resources section
3. QML files are automatically compiled into resources

### Modifying Dependencies

1. Update `CMakeLists.txt` for CMake dependencies
2. Update `package.nix` for Nix package dependencies
3. Rebuild with `just nix-build` to test Nix changes

## Git Workflow

### Creating Patches

```bash
# Create a patch from staged changes
just git-create-patch
```

### Applying Patches

```bash
# Apply a patch to the project
just git-apply-patch
```

Patches are stored in `~/NextcloudPrivate/Transfer/` or `~/Nextcloud/Transfer/`.

## Debugging

- Use `--debug` flag when running: `./build/nixbit --debug`
- Check `build/compile_commands.json` for IDE integration
- Qt Creator or CLion recommended for C++ development

## Important Files to Preserve

When making changes, be careful with:

- `version.h.in` - Template for version information
- `CMakeLists.txt` - Build configuration
- `flake.nix` - Nix flake definition
- `package.nix` - Package definition
- `justfile` - Build commands

## Qt/QML Integration

- Qt resources are auto-generated (CMAKE_AUTORCC)
- Meta types are generated in `build/meta_types/`
- QML type registrations are auto-generated
- Use `qt6.wrapQtAppsHook` in Nix for proper Qt plugin paths

## Installation

```bash
# Install via CMake (system-wide, may require sudo)
just install

# Install via Nix to user profile
just nix-install
```

## Testing the Nix Package

Always verify Nix builds:

```bash
# Build and check
just nix-build
just nix-check

# Show build result
just nix-result
```

## Notes for AI Assistants

1. **Always prefer `just` commands** over raw cmake/nix commands
2. When modifying C++ code, check if Qt MOC is needed (Q_OBJECT, signals, slots)
3. QML property bindings should use proper Qt Quick syntax
4. Nix builds are pure - don't rely on system dependencies
5. The app requires a display server - cannot run headless without Xvfb
6. Version bumps need to be done in CMakeLists.txt only
7. Use `just nix-build` to verify Nix package builds correctly after changes
