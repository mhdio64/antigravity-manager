# Antigravity Manager

[فارسی](README.md)

Antigravity Manager is a graphical Linux application for installing and managing **Antigravity App** and **Antigravity IDE**. You can install and update either product independently.

## Why it exists

When you download Antigravity as a Linux archive, you have to extract it, choose where to keep it, and create a launcher if you want it to appear with its icon in your applications menu. Updating means finding the new archive, downloading it, and replacing the current installation. Rolling back depends on whether you kept the old files.

Antigravity Manager handles those steps in one window. Click **Install** on the product you want. The manager downloads the official Google archive, installs it for your Linux user, and registers its name and icon in the applications menu. You can then open it from the menu or use the **Launch** button.

## What it does

- **Check for updates.** Each product shows its own installation and update status. When the installed version matches the version available from the official source, the manager shows **Up to date**.
- **Update and roll back.** The manager prepares the new version and waits for the product to close before switching versions. It keeps one previous version that you can restore.
- **Remove.** Removal deletes the managed installation, its previous version, and its menu entry while keeping your settings and projects.
- **Enable right-to-left text.** Optional RTL support can be enabled or removed separately for compatible builds of App and IDE. It requires Node.js 22.12 or newer.

The manager is available in English and Persian. Google sign-in takes place in Antigravity itself, and Google determines access to the service.

## Download and install

Get the appropriate `.deb` from [GitHub Releases](https://github.com/mhdio64/antigravity-manager/releases) and open it with your system's package installer. These packages are for 64-bit Intel and AMD systems.

| System | Package |
| --- | --- |
| Ubuntu or Kubuntu 26.04 | File ending in `_resolute_amd64.deb` |
| Ubuntu 24.04 | File ending in `_amd64.deb` without `resolute` |

Both packages provide the same features. They use the Qt libraries for their respective Ubuntu versions. Releases are currently previews. The Ubuntu 24.04 package is still being evaluated on Linux Mint 22.

To build from source, see the [developer guide](prototype/README.md).
