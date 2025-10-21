# Nixbit Changelog

## 0.1.2

- When you change the repository URL, the existing repository will be removed
  and re-cloned after a confirmation to avoid issues with mismatched repositories
  (for [#3](https://github.com/pbek/nixbit/issues/3))

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
