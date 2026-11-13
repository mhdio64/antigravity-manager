# Release verification

The 0.8.0 packages target Linux amd64. Build with the system Qt for each base. The Ubuntu 24.04 package requires Qt library names that Ubuntu 26.04 no longer provides. Use the package with the `resolute` suffix on Ubuntu or Kubuntu 26.04.

| Environment | Package | Evidence |
| --- | --- | --- |
| Ubuntu 24.04 container | `antigravity-release-check_0.8.0_amd64.deb` | Package install, offscreen startup and live App download resumed from an ETag-matched partial |
| Ubuntu 26.04 container | `antigravity-release-check_0.8.0_resolute_amd64.deb` | Clean package installation, both live release checks and offscreen startup passed |
| Kubuntu 26.04 / KDE | Resolute package | Manager window started on the KDE X11 display from the Ubuntu 26.04 runtime container. Native desktop menu and product launches still require interactive verification |
| Linux Mint 22 / Cinnamon | Ubuntu 24.04 package candidate | Fresh installation and desktop checks pending |

For a clean test, choose an isolated XDG data and config directory. Exercise both products independently from installation through removal. Check the system menu and icon, update while the product is running, restore a previous version, and confirm settings and projects survive removal. Verify Persian layout, keyboard navigation, report export and RTL behavior inside each product.

The self-update check expects a stable public GitHub release tagged `vMAJOR.MINOR.PATCH` and the exact package filename for the installed distribution build. The download opens in the browser for installation with the system's package tool.

## Remaining acceptance work

- Run the complete matrix in a clean Linux Mint desktop and a native KDE session, including RTL rendering inside each product and the report file dialog.
- Existing-install detection covers common locations and offers a fresh managed copy. Migration of an arbitrary existing installation and its customized configuration is not implemented.
- Settings are preserved during removal. Optional settings cleanup and automatic recovery of rollback metadata after interruption are pending.
- Verify the manager's update button against a real stable release and the two distribution-specific assets. Version 0.8.0 is a local candidate.
