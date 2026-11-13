# Antigravity release checker

This Qt Quick prototype checks Linux x64 releases and installs either product for the current user. Use the Install button to download and extract the archive, then start the product from the desktop application menu. The installer registers the icon supplied with each product and rejects duplicate managed installs.

Choose **English** or **فارسی** from the language selector. The manager saves the choice locally and uses the system language as the default on first launch. Persian mirrors the interface while keeping download links and version numbers readable left to right.

Each installed product shows its installation folder and can open it in the file manager. From a terminal, run `antigravity-release-check --launch app` or `antigravity-release-check --launch ide`. Managed installations register their names and icons in the desktop application menu under the current XDG data directory.

**Enable RTL** and **Remove RTL** change the selected Antigravity product while it is closed. They require Node.js 22.12 or newer on the manager's PATH. The package embeds [Antigravity RTL 1.4.1](https://github.com/mmnaderi/antigravity-rtl/tree/a968b1a7a6cb82910c3cb273789cd72f96b14344) modules and their locked dependencies. The manager calls the modules on a staged copy and activates that copy only after success. The compatibility gate currently accepts the main files from App 2.31.2 and IDE 2.7.0 Linux x64. Each new product version needs a separate compatibility check. The new active version after a product update starts without RTL.

Before configuring CMake, run `npm --prefix prototype/rtl ci --omit=dev --ignore-scripts` with Node.js 22.12 or newer. For CLI verification on managed installations use `--apply-rtl app` / `--remove-rtl app` or replace `app` with `ide`.

When RTL is active or pending, the GUI asks for confirmation before updating. The update keeps the RTL preference on the new installation, while leaving RTL off until the user chooses **Re-enable RTL**. Known unsupported version numbers disable that action, and the installer checks the actual file fingerprint before any change.

Build on Ubuntu 24.04 amd64 with CMake, a C++ compiler, dpkg-dev, file, libarchive-dev, qt6-base-dev and qt6-declarative-dev:

```sh
cmake -S prototype -B build/release-check
cmake --build build/release-check
cpack --config build/release-check/CPackConfig.cmake -B build/packages
build/release-check/antigravity-release-check --check
```

The command exits with status 1 if either release check fails. Launch without arguments for the desktop view. The package declares its QML runtime modules.

For isolated verification, set `XDG_DATA_HOME` to a temporary directory. Use `--install-latest app` or `--install-latest ide` to exercise the same download and update flow as the GUI. For a trusted local archive, use `--install-archive` for the first installation or `--update-archive` for an existing installation, followed by the product and archive path. Use `--rollback app` or `--rollback ide` to swap the active and previous versions.

Update and rollback wait up to five minutes for processes running from the managed install to exit. They use Linux directory exchange and require a filesystem supporting it. The desktop entry keeps the same path across a switch. Interrupted downloads resume when the server provides a matching strong ETag and a valid range response. The prototype does not yet recover rollback metadata automatically after abrupt interruption.

Use **Remove** and confirm, or run `--remove app` or `--remove ide`. Close the product before removing it. Removal deletes its managed active and previous versions and desktop entry. Settings and projects are preserved. Optional settings cleanup is pending.

**Save support report** exports up to 100 recent events from the current session, with timestamps, operation summaries and version numbers. The application does not send these reports.

For Ubuntu/Kubuntu 26.04, build on 26.04 with `-DPACKAGE_VARIANT=resolute` and use the package with the `resolute` suffix. See the [release verification matrix](../docs/release-verification.md).

The two JSON feeds and archive URL patterns come from the [official release page](https://antigravity.google/releases). They are site implementation details. The checker expects the newest entry first, as the site does, and reports a failed check if the response or its fields are invalid. Compare its output against the [official download page](https://antigravity.google/download) when validating a new release.
