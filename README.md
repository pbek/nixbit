# Nixbit

A **GUI application for updating your NixOS system** from a Nix Flakes Git repository.

You can try it out by running:

```bash
nix run github:pbek/nixbit
```

> [!TIP]
> If Qt complains about different minor versions, you can try using your own nixpkgs:
>
> ```bash
> nix run github:pbek/nixbit --override-input nixpkgs nixpkgs
> ```

![Nixbit Screenshot](nixbit.webp)

There also is a **NixOS Module** to allow the configuration of the Git repository,
so you can preset it for all systems in your fleet.

## NixOS Module

The flake includes a NixOS module to configure nixbit system-wide:

```nix
# In your flake.nix inputs
inputs.nixbit.url = "github:pbek/nixbit";
inputs.nixbit.inputs.nixpkgs.follows = "nixpkgs";

# In your NixOS configuration
{
  imports = [ inputs.nixbit.nixosModules.nixbit ];

  services.nixbit = {
    enable = true;
    repository = "https://github.com/youruser/nixcfg.git";
  };
}
```

This will install nixbit and create a configuration file at `/etc/nixbit.conf` with the specified repository URL.

## Features

### Repository Management

- **Repository URL Configuration**: Input field for Git repository URLs with confirmation dialog for changes
- **Local Repository Management**: Display local path, delete repository with safety checks and confirmation, open terminal in repository directory
- **Status Monitoring**: Real-time display of repository status, commits behind, and busy indicators
- **Auto-fetch Interval**: Configurable automatic fetch interval in minutes

### System Update

- **Hostname Configuration**: Input field for NixOS system hostname
- **Rebuild Mode Selection**: Choose between 'build' (no activation) and 'switch' (build and activate) modes
- **Update System**: One-click button to pull repository updates and rebuild the system
- **Check for Updates**: Button to manually check for repository updates

### User Interface

- **Modern KDE Integration**: Built with Kirigami for native KDE Plasma look and feel
- **Menu Bar**: File menu with Quit option, Tools menu with Check for Updates
- **Action Buttons**: Quick access to system update and update check operations
- **Terminal Output Panel**: Real-time command output display with clear and kill process buttons
- **Progress Indicators**: Progress bar for cloning operations and busy indicators for ongoing tasks
- **System Tray Support**: Option to start the application hidden in the system tray
- **Confirmation Dialogs**: Safety prompts for deleting repositories and changing URLs
- **Status Notifications**: Inline messages for operation results and errors

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

This project uses `devenv` for a reproducible development environment with all necessary dependencies:

```bash
# Enter the development shell
devenv shell

# Or use direnv (if configured)
direnv allow
```

### Using Just Recipes

The project provides Just recipes for common build and development tasks:

```bash
# Configure the project with CMake
just build

# Run the application
just run

# Build the nix package
just nix-build

# Run the application from the nix package
just nix-run
```

## License

See [LICENSE.md](LICENSE.md) for details.

## Contributing

This is an early-stage project. Contributions are welcome!

---

Built with ❤️ for the NixOS community
