# Nixbit

A GUI application for updating your NixOS system from a Nix Flakes Git repository.

![Nixbit Screenshot](nixbit.webp)

## Overview

Nixbit is a Qt6/QML-based desktop application that provides a graphical interface for updating NixOS systems from Git repositories. Built with KDE Kirigami for a modern, responsive UI.

Run directly with Nix:

```bash
nix run github:pbek/nixbit#nixbit
```

## Features

### Git Repository Management

- **Clone and Pull Operations**: Clone new repositories or pull updates from existing ones
- **Repository URL Configuration**: Easy input field for Git repository URLs
- **Status Monitoring**: Real-time Git repository status display
- **Local Path Management**: Automatic handling of repository local paths

### Process Management

- **Command Execution**: Run arbitrary commands with arguments
- **Directory-Aware Execution**: Execute commands in specific working directories
- **Real-time Output**: Live terminal output display with both stdout and stderr
- **Process Control**: Kill running processes when needed
- **System Information**: Retrieve hostname and system details

### User Interface

- **Modern KDE Integration**: Built with Kirigami for native KDE Plasma look and feel
- **Menu Bar**: File and Tools menus for application management
- **Repository Configuration Panel**: Input field for repository URLs
- **Action Buttons**: Quick access to Pull, Clone/Pull, and Rebuild System operations
- **Terminal Output Panel**: Collapsible terminal window showing command output and status
- **Status Feedback**: Visual indicators for busy states and operation results

## Technology Stack

- **Language**: C++ (Qt6)
- **UI Framework**: QML with KDE Kirigami
- **Build System**: CMake 3.20+
- **Dependencies**:
  - Qt6 (Core, Gui, Qml, Quick, Widgets)
  - KDE Frameworks 6
  - KF6 Kirigami
  - Git (runtime dependency)

## Building

### Prerequisites

```bash
# On NixOS, you can use the provided flake:
nix develop

# Or build directly:
nix build
```

### Manual Build

```bash
mkdir build
cd build
cmake ..
make
```

The executable will be created as `nixbit` in the build directory.

## Architecture

### Core Components

#### GitManager

A QObject-based class that handles Git operations:

- **Properties**:
  - `repositoryUrl`: The Git repository URL
  - `localPath`: Local filesystem path for the repository
  - `status`: Current status messages
  - `isBusy`: Indicates if an operation is in progress

- **Methods**:
  - `pullRepository()`: Pull updates from remote
  - `cloneOrPullRepository()`: Clone if not exists, otherwise pull

- **Signals**:
  - `operationCompleted(bool success, QString message)`: Emitted when operations finish

#### ProcessManager

A QObject-based class for executing system commands:

- **Properties**:
  - `output`: Accumulated command output
  - `isRunning`: Process execution state

- **Methods**:
  - `runCommand(program, args)`: Execute a command
  - `runCommandInDirectory(program, args, workingDir)`: Execute in specific directory
  - `killProcess()`: Terminate running process
  - `getHostname()`: Retrieve system hostname

- **Signals**:
  - `commandFinished(int exitCode, QString output)`: Emitted when command completes

## Development

### Development Environment

This project uses `devenv` with Nix flakes for a reproducible development environment:

```bash
# Enter development shell
devenv shell

# Or use direnv (if configured)
direnv allow
```

### Project Structure

```
nixbit/
├── src/
│   ├── main.cpp              # Application entry point
│   ├── main.qml              # Main UI definition
│   ├── gitmanager.h/cpp      # Git operations manager
│   └── processmanager.h/cpp  # Process execution manager
├── CMakeLists.txt            # Build configuration
├── flake.nix                 # Nix flake definition
├── package.nix               # Nix package definition
└── devenv.nix                # Development environment
```

## Use Cases

### NixOS System Updates

1. Enter your NixOS configuration Git repository URL
2. Click "Clone/Pull Repository" to fetch the latest configuration
3. Click "Rebuild System" to apply the new configuration
4. Monitor the process in the terminal output panel

### General Git Repository Management

- Use as a simple Git client for any repository
- Monitor repository status
- Execute custom commands in repository directories

## License

See [LICENSE.md](LICENSE.md) for details.

## Version

Current version: 1.0.0

## Contributing

This is an early-stage project. Contributions are welcome!

---

Built with ❤️ for the NixOS community
