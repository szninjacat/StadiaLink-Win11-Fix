<div align="center">
  <img src="app/assets/readme-logo.svg" width="112" height="112" alt="StadiaLink Rev logo">
  <h1>StadiaLink Rev</h1>
  <p><strong>Your Stadia. Your way.</strong></p>
  <p>Stadia controller configuration for Windows 11 x64 · Version 1.0.0</p>
  <p><a href="https://github.com/szninjacat/StadiaLink-Rev/releases/latest"><strong>⬇ Download</strong></a> · <a href="https://github.com/szninjacat/StadiaLink-Rev/issues">Report an issue</a> · <a href="LICENSE">GPL-3.0</a></p>
  <p>🪟 Windows 11 x64 &nbsp; · &nbsp; 🎮 XInput &nbsp; · &nbsp; 🔗 USB + Bluetooth</p>
  <p><a href="README.es.md"><img src="app/assets/flag-es.svg" width="24" height="16" alt="ES"> Español</a> &nbsp; | &nbsp; <strong><img src="app/assets/flag-en.svg" width="24" height="16" alt="EN"> English</strong> &nbsp; | &nbsp; <a href="README.fr.md"><img src="app/assets/flag-fr.svg" width="24" height="16" alt="FR"> Français</a> &nbsp; | &nbsp; <a href="README.de.md"><img src="app/assets/flag-de.svg" width="24" height="16" alt="DE"> Deutsch</a> &nbsp; | &nbsp; <a href="README.zh-CN.md">🇨🇳 简体中文</a></p>
</div>

> **StadiaLink Rev 1.0.0** — an independent configuration tool for the Stadia controller on Windows 11 x64. It restores Bluetooth rumble on build 22631, ships a Simplified Chinese UI by default, keeps the app in the tray when the window is closed and validates private GATT interfaces on untested Windows builds. See [RELEASE_NOTES.md](RELEASE_NOTES.md) and [docs/CHANGES.md](docs/CHANGES.md).
>
> **Independent release line** — `1.0.0` is the first version of this project. Update checks only use stable releases from this repository; no external project can trigger an update prompt.
>
> **Origin** — StadiaLink Rev is an independent derivative of [minysito/StadiaLink](https://github.com/minysito/StadiaLink) (GPL-3.0). Its own version line starts at 1.0.0 and its update checks never consult the original project. See [CREDITS.md](CREDITS.md).

---

## 🎮 Features

- USB and Bluetooth connections with XInput output.
- Button mapping, keyboard shortcuts and push to talk.
- Stick sensitivity, dead zones and trigger adjustments.
- Per-motor vibration and a continuous test with manual stop.
- Importable and exportable profiles.
- Battery level, notifications, system tray and Windows startup.
- Responsive interface in Simplified Chinese (default), Spanish, English, French and German.
- Bluetooth rumble repaired on Windows 11 23H2, with runtime interface validation on other builds.
- Closing the window (or Alt+F4) minimizes to the tray; the tray menu's **Exit** ends the process.

## 🚀 Installation

Download `StadiaLink.Setup.exe` from [Releases](https://github.com/szninjacat/StadiaLink-Rev/releases/latest), run it and approve the administrator prompt for driver installation. Connect your controller by USB or pair it in Windows Bluetooth settings and turn it on. StadiaLink Rev detects the available controller.

Bluetooth requires the controller's Bluetooth firmware. StadiaLink Rev does not modify firmware. The installer includes its components and corresponding source code and works offline. It uses .NET Framework 4.8 and Windows components; no third-party controller emulator or additional runtime is required.

Prebuilt binaries are also committed under [dist/](dist/). Before installing over a running copy, exit it from the tray menu (the single-instance lock would block the installer). If the controller does not appear afterwards, toggle Bluetooth or reconnect it.

Compare the download hash with `SHA256SUMS.txt` from the same release:

```powershell
Get-FileHash .\StadiaLink.Setup.exe -Algorithm SHA256
```

## 🛠 Build from scratch

### Requirements

- Windows 11 x64 and the included Windows PowerShell 5.1.
- [Git for Windows](https://git-scm.com/downloads/win), or GitHub's source ZIP.
- [Visual Studio 2022 or Build Tools 2022](https://visualstudio.microsoft.com/downloads/) with **Desktop development with C++**, MSVC x64 and Windows SDK **10.0.26100.0**.
- .NET Framework **4.8**. Scripts use `%WINDIR%\Microsoft.NET\Framework64\v4.0.30319\csc.exe` and Windows WPF libraries. Node.js and `dotnet build` are not required.
- Internet for the first driver build. The script downloads `Microsoft.Windows.WDK.x64` **10.0.26100.6584** from NuGet, uses UMDF **2.31** and caches the WDK in `.build/driver/.wdk`.

Open Windows PowerShell in a working folder. Building does not require administrator privileges; installing the driver does.

```powershell
git clone https://github.com/szninjacat/StadiaLink-Rev.git
cd StadiaLink-Rev

# If scripts are blocked; applies only to this session.
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned

# Driver, application, native library and installer.
.\app\package.ps1 -Driver

# Input translation, IID policy and application model tests.
.\tests\run.ps1
```

If using the ZIP, extract it and open PowerShell in the folder containing `app`, `driver` and `tests`. Review scripts before unblocking them if Windows marks them as Internet downloads. The example does not change the machine's permanent execution policy.

| Path | Output |
| --- | --- |
| `release/StadiaLink.exe` | x64 WPF application. |
| `release/StadiaLink.exe.config` | .NET Framework configuration. |
| `release/StadiaDevice.dll` | Native battery reader. |
| `release/StadiaLink.Setup.exe` | Installer with components and embedded `Source.zip`. |
| `release/StadiaLink.Setup.exe.config` | Installer configuration. |
| `release/SHA256.txt` | Hash of your locally built installer. |
| `driver/out/pkg/` | Compiled UMDF driver package. |
| `.build/` | Dependencies, object files and test results. |

Run `release/StadiaLink.Setup.exe` to install your build. Running the application alone does not install the driver. For separate build steps:

```powershell
.\driver\build.ps1       # Driver only.
.\app\build.ps1          # Application and native library.
.\app\package.ps1        # Installer using the existing driver build.
.\tests\run.ps1          # Requires the built application.
```

If MSVC is missing, check Visual Studio's C++ workload. Missing headers or libraries usually require the specified SDK. Check NuGet access if the WDK download fails. Driver builds include a build date and version; binaries are not necessarily identical between builds.

## ⚙️ Usage

Vibration gain applies and saves as you move its sliders. For buttons, language and other profile changes, click **Save and apply**. Switching tabs discards unsaved profile changes. The Windows startup checkbox saves immediately and starts StadiaLink Rev in the tray when that user signs in. Continuous vibration stops with **Stop**, on tab changes, disconnection or app exit.

Keyboard shortcuts require the app to remain open, including in the tray. Configure the same push-to-talk key or combination in your game or voice app. The driver retains the applied XInput configuration when the interface closes.

Profiles are stored at `%LOCALAPPDATA%\StadiaStudio\profiles.json`; the installation folder is `%LOCALAPPDATA%\Programs\StadiaStudio`. These internal names are part of the app's storage layout.

## 🔋 Battery detection

No firmware activation or separate utility is needed. After detecting a controller, `StadiaDevice.dll` reads its percentage in the background every **30 seconds**, displaying it in the interface and tray icon.

- **Bluetooth:** Windows must enumerate the paired controller's BLE Battery Service (`0x180F`). The library reads Battery Level (`0x2A19`) directly from that device and validates a one-byte value from 0 to 100. It does not write to the battery service or modify HID.
- **USB:** the library locates the Stadia WinUSB interface `VID_18D1 / PID_9400 / MI_00`, requests the value with control request `0x83` and retrieves it with `0x84`, validating its format and percentage. It does not write firmware.
- **Charging:** USB connection displays **Charging**, or **Fully charged** if the returned level is 100 %. This is inferred from the connection; it does not measure charging current or confirm physical charging.
- **Notifications:** when enabled, connection, disconnection, battery at **15 % or below**, and **100 %** generate notifications. Full-battery notification requires a valid reading.

**Battery unavailable** means the level could not be read; no percentage is invented. On Bluetooth, power-cycle the controller and wait for another reading. If needed, remove and pair it again so Windows re-enumerates its services. On USB, check the data connection and WinUSB interface availability. Working buttons do not guarantee access to battery data.

## 🔄 Updates

The app checks GitHub at startup and every six hours. Only stable, non-draft releases of `szninjacat/StadiaLink-Rev` with a plain `v`-prefixed version tag are considered; the page URL and API URL must both belong to this repository. No other repository is queried and no external project can trigger a notification. Downloads and installation are always manual from **Settings**.

`StadiaLink.Setup.exe` installs the application, native library and driver. `StadiaLink.Update.exe` refreshes only the application, preserves profiles and requires a compatible installed driver; otherwise use the full installer.

## 📦 Repository layout

| Path | Contents |
| --- | --- |
| `dist/` | Prebuilt release binaries, driver package and SHA-256 checksums. |
| `app/` | WPF application, installer, resources and build scripts. |
| `driver/` | UMDF driver sources, INF, installation and status scripts. |
| `tests/` | Test suites and private-GATT probe tools. |
| `docs/CHANGES.md` | Implementation notes. |
| `docs/COMPATIBILITY.md` | Compatibility matrix and interface-validation strategy. |
| `RELEASE_NOTES.md` | Full release notes for 1.0.0. |
| `CREDITS.md` | Third-party code attribution. |

## 💬 Issues, contributions and removal

Open an [issue](https://github.com/szninjacat/StadiaLink-Rev/issues) with Windows version, app version, connection type and reproduction steps. Installation logs are at `%LOCALAPPDATA%\StadiaStudio\logs\driver-install.log`; remove personal information before sharing them.

Pull requests are welcome. Describe your checks and run `tests/run.ps1` when changing configuration or input translation. Do not commit downloaded dependencies, generated binaries, personal profiles or firmware research material.

Uninstall through Windows **Installed apps**. To remove only the driver from a built checkout:

```powershell
.\driver\install.ps1 -Uninstall
```

This requests administrator privileges, removes this driver's packages and returns the controller to Windows' built-in HID driver. Reconnection may be needed.

## 🤝 Compatibility, credits and license

Bluetooth vibration uses private Windows interfaces, verified on real machines only on Windows 11 builds **26200** and **22631**. Other builds — including Windows 10 — are installable at the UMDF 2.31 framework level only; the private GATT rumble interface is unverified there and not guaranteed. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) for the matrix and validation procedure. On other builds the driver automatically validates and caches a runtime-discovered device IID (vtable checks, full GATT-chain validation and a crash circuit breaker); success is not guaranteed. System updates may also change these interfaces. The audio jack works over USB. Bluetooth audio, integrated microphone activation and Wi-Fi activation are not provided. Timing figures measure HID queries and interface updates, not total physical latency through a game and display.

**Project provenance: [minysito/StadiaLink](https://github.com/minysito/StadiaLink).** StadiaLink Rev is an independent derivative of that GPL-3.0 project: the application, installer, translations, documentation and build scripts come from it, with StadiaLink Rev's own modifications. The version line restarts at 1.0.0 and update checks read only stable releases of this repository, so releases or version numbers of the original project never trigger an update prompt. Original copyright notices and the GPL-3.0 license are retained; see [LICENSE](LICENSE) and [CREDITS.md](CREDITS.md). This project is not affiliated with or endorsed by the original author, Google or Microsoft.

**Incorporated third-party code: [aisk/WinStadia](https://github.com/aisk/WinStadia).** It is the base of the native driver included in this repository, with StadiaLink Rev modifications. The integration covers the UMDF structure, Stadia-to-XInput input translation and USB/Bluetooth transports in `driver/`, along with derived scripts and INF.

Driver source project: **https://github.com/aisk/WinStadia**.

Reference commit: [e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a](https://github.com/aisk/WinStadia/commit/e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a). Copyright notices are retained. StadiaLink Rev and its modifications are distributed under **GPL-3.0**; see [LICENSE](LICENSE) and [CREDITS.md](CREDITS.md). This project is independent of Google and Microsoft.
