# Credits, provenance and third-party code

## Origin project: minysito/StadiaLink

StadiaLink Rev is an **independent derivative** of [minysito/StadiaLink](https://github.com/minysito/StadiaLink), distributed under GPL-3.0. The application, installer, translations, documentation and build scripts in this repository originate from that project, with StadiaLink Rev's own modifications. The native UMDF driver originates from [aisk/WinStadia](https://github.com/aisk/WinStadia), described below. Copyright notices of the original authors and the GPL-3.0 license are retained in [LICENSE](LICENSE) and in the source files.

"Independent" describes the release and maintenance model, not the provenance of the code:

- The version line restarts at **1.0.0**. It is not derived from, compared with or aligned to the original project's version numbers.
- Update checks read only stable, non-draft releases of `szninjacat/StadiaLink-Rev`. Releases, tags and version numbers of the original project never trigger an update prompt in StadiaLink Rev.
- The repository is maintained separately. Issues, pull requests, release assets and the roadmap of the original project are not shared with this one, and no upstream updates are applied automatically.

This project is not affiliated with, endorsed by or maintained by the original author, Google or Microsoft.

## aisk/WinStadia

Source project: https://github.com/aisk/WinStadia

Reference commit: https://github.com/aisk/WinStadia/commit/e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a

The native UMDF driver distributed with StadiaLink Rev is derived from WinStadia and is included with modifications. The incorporated base covers the UMDF structure, Stadia-to-XInput input translation, the USB/Bluetooth transports and the installation and build scripts in `driver/`.

Derived files: `winstadia.c`, `winstadia.h`, `stadia.c`, `usb.c`, `bluetooth.c`, `winstadia.inf`, `build.ps1`, `install.ps1` and `status.ps1`. The StadiaLink Rev modifications and all additional files are part of the distributed source code.

License: GPL-3.0. Applicable original copyright notices are retained. StadiaLink Rev and its modifications are distributed under GPL-3.0; see [LICENSE](LICENSE).

This document records the origin of the project and the code incorporated into the distributed application and driver. StadiaLink Rev is independent of Google and Microsoft.
