# Credits and third-party code

## aisk/WinStadia

Original project: https://github.com/aisk/WinStadia

Reference commit: https://github.com/aisk/WinStadia/commit/e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a

The native UMDF driver distributed with StadiaLink Rev is derived from WinStadia and is included with modifications. The incorporated base covers the UMDF structure, Stadia-to-XInput input translation, the USB/Bluetooth transports and the installation and build scripts in `driver/`.

Derived files: `winstadia.c`, `winstadia.h`, `stadia.c`, `usb.c`, `bluetooth.c`, `winstadia.inf`, `build.ps1`, `install.ps1` and `status.ps1`. The StadiaLink Rev modifications and all additional files are part of the distributed source code.

License: GPL-3.0. Applicable original copyright notices are retained. StadiaLink Rev and its modifications are distributed under GPL-3.0; see [LICENSE](LICENSE).

This document records code incorporated into the distributed application and driver. It is not a bibliography of projects consulted during research. StadiaLink Rev is independent of Google and Microsoft.
