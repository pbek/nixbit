# Nixbit Changelog

## 0.1.3

- There now is a checkbox to allow to create or remove an autostart desktop entry
  (for [#6](https://github.com/pbek/nixbit/issues/6))
  - The autostart entry can now be forced to be created with the external config file `/etc/nixbit.conf`
  - There also is a new nix module option `services.nixbit.forceAutostart` for this
- Fix warnings caused by the app icon SVG file (for [#2](https://github.com/pbek/nixbit/issues/2))
- Adapt the window title a bit to show version number at the end
- Wait for network availability after system resume before checking for updates,
  with a 10-second timeout to prevent failures when network is not immediately present
  (for [#7](https://github.com/pbek/nixbit/issues/7))

## 0.1.2

- When you change the repository URL, the existing repository will be removed
  and re-cloned after a confirmation to avoid issues with mismatched repositories
  (for [#3](https://github.com/pbek/nixbit/issues/3))
- Don't allow the repository URL to be changed, if it was overwritten by the
  external config file (for [#5](https://github.com/pbek/nixbit/issues/5))

## 0.1.1

- The repository url can now be overwritten with an external config file `/etc/nixbit.conf`
  (for [#5](https://github.com/pbek/nixbit/issues/5))
  - This can be done by a nix module or other system configuration management tools
- The repository path will now always be the same and does not depend on the git repository name

## 0.1.0

- Initial release of Nixbit
- NixOS system update functionality
- User interface built with Qt/QML
- Setup of development environment with Nix flakes and devenv
- Terminal output panel for monitoring commands
- Tray icon support
- Automatic fetching of Git repository updates
- Settings for repository URL, hostname, fetch interval and hiding to tray
