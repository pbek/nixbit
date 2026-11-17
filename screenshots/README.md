# Nixbit Screenshots

This directory contains screenshots showcasing the various features and interfaces of Nixbit.

Regenerate them with `just screenshots-vm` (including window borders) or
`just screenshots --framed` on Plasma, then review and
publish with `just screenshots-publish <capture-directory>`.
See [automatic screenshot documentation](../docs/screenshots.md) for scene coverage,
requirements and debugging.

## Main Window

The main window provides the primary interface for managing your NixOS system updates.

![Nixbit Main Window](mainwindow.webp)

## System Monitor

Real-time monitoring of CPU, memory, network, and disk I/O during system builds.

![Nixbit System Monitor](monitor.webp)

## Settings Dialog

Configuration interface for build hosts, repository settings, and application preferences.

![Nixbit Settings Dialog](settings.webp)

### Repository and Build Hosts

![Repository settings](settings-repository.webp)

![Remote build hosts](settings-hosts.webp)

### Notifications, Performance and Debug Settings

![Notification settings](settings-notifications.webp)

![Performance settings](settings-performance.webp)

![Debug settings](settings-debug.webp)

## Available Updates

Review new commits, authors and changes before updating the system.

![Available updates](commits.webp)

## NixOS Generations

Generation history identifies the running system and the next boot default.

![NixOS generations](generations.webp)

## Build Logs

Stored build, switch and boot logs show successful and failed operations.

![Build logs](logs.webp)

## User Packages

Manage packages installed with `nix profile add`, separately from the system
configuration. The screenshots use illustrative demo packages and versions.

### Installed Packages and Updates

![Installed user packages and available updates](packages.webp)

### Expanded Package Details

![Package metadata, source revision and store paths](packages-details.webp)

### Find Packages

Search nixpkgs and see which packages are already provided by the system.

![Package search](packages-search.webp)

### Remove a Package

![Package removal confirmation](packages-remove.webp)

### Update Progress

![Package update progress and output](packages-progress.webp)

### Unfree Licenses and File Conflicts

Retry an installation with an unfree license allowed or a lower priority.

![Unfree package retry](packages-unfree.webp)

![File conflict retry](packages-conflict.webp)

## About Nixbit

![About Nixbit](about.webp)
