# Implementation notes

This document describes what is in the repository and why. The source trees under `app/`, `driver/` and `tests/` are the complete, buildable codebase — no external patch has to be applied.

## 1. Driver (`driver/`)

| File | Role |
| --- | --- |
| `private-gatt.cpp` | Private WinRT/GATT rumble transport: activation factory, verified device IIDs for builds 26200 and 22631, runtime candidate resolution, `CreateDeviceObject` / `TryOpenWithDevice` / `GuardedTryOpen` / `AutoAdapt`, async stage reporting and registry-backed caching |
| `private-gatt.h` | Stage and async-stage enumerations, `Stage`, `IidIndex`, `CandidateCount`, `AsyncStage` diagnostic fields |
| `iid-policy.h` | Pure policy header shared with unit tests: candidate filtering, verified-IID exclusion, blacklist rotation, diagnostic bit encoding |
| `winstadia.c` | UMDF device: status feature report, input translation, vibration, USB/Bluetooth transports. Bytes 60–63 of the status report (previously unused) carry the diagnostics above; report length and existing offsets are unchanged |
| `status.ps1` | Reads the status report and prints the stage, IID source, candidate count and async failure point |
| `build.ps1` | WDK/MSVC driver build; `STADIALINK_VC_ENV_READY=1` skips `vcvars64.bat`; `STADIALINK_AUTO_IID=0` builds the strict fail-closed variant (default is validated adaptation) |
| `install.ps1`, `winstadia.inf` | Driver installation/uninstallation and INF with fixed `VID_18D1&PID_9400` match |

The two verified IIDs take the original code path without any new logic. Only unknown builds enter candidate filtering → static vtable validation → full GATT-chain semantic validation → SEH isolation → caching/blacklist handling. Registry state lives under `HKCU\SOFTWARE\StadiaLink\PrivateGatt`; if it cannot be written, persistence is skipped silently. USB rumble, input translation, configuration and battery handling are untouched.

## 2. Application (`app/`)

| File | Role |
| --- | --- |
| `App.cs` | Single-instance startup, self-test modes, tray lifecycle; test exits use `ForceClose()` |
| `MainWindow.cs` | WPF UI, profile editing, vibration sliders, tray integration. **×** and Alt+F4 hide the window (`ShowInTaskbar=false`); `ForceClose()` sets a flag so `Closing` does not cancel |
| `I18n.cs` | Five-language catalog loader; default and fallback are `zh` (Simplified Chinese) |
| `assets/locales.tsv` | 289 entries × 5 columns (es / en / fr / de / zh), no empty values |
| `UpdateChecker.cs` | Release checks restricted to `szninjacat/StadiaLink-Rev`; validates page and API URLs, HTTPS, non-draft/non-prerelease and a plain `v`-prefixed version tag before comparing versions |
| `InstallerActions.cs` | Install/uninstall registry entry, shortcuts, driver script invocation |
| `Setup.cs`, `UpdatePackage.cs`, `Model.cs`, `Ui.cs` | Installer, application-only updater, profiles/configuration and shared WPF helpers |
| `package.ps1` | Builds the installer and updater, embeds `payload.zip` and `Source.zip`, writes SHA-256 manifests |
| `build.ps1`, `build-native.ps1` | WPF application and native battery library builds |
| `app.manifest`, `StadiaLink.ico`, `assets/` | Manifest (`1.0.0.0`), icon, logos, flags and translation catalog |
| `USO.md` | Short end-user guide embedded in the installer |

Assembly, file and manifest versions are `1.0.0.0`. The uninstall entry reports `StadiaLink Rev` with the version read from the assembly. Profile storage (`%LOCALAPPDATA%\StadiaStudio`) and the installation directory are unchanged.

## 3. Tests and tools (`tests/`)

| File | Role |
| --- | --- |
| `configuration-tests.cpp` | Input translation and configuration checks (4,864,527 assertions) |
| `iid-policy-tests.cpp` | 26 assertions over candidate filtering, variant bits, verified-IID exclusion, blacklist rotation and diagnostic encoding |
| `build-zh-locale.py` | Idempotent generator for the five-column translation catalog |
| `probe-private-gatt.py` | Read-only enumeration of private interface IIDs on a new Windows build; never connects to a controller or writes GATT characteristics |
| `run.ps1` | Compiles and runs the C++ suites, then the application model and live vibration slider tests |

## 4. Documentation and deliverables

- `README.md` (English default), `README.zh-CN.md`, `README.es.md`, `README.fr.md`, `README.de.md`, `README.en.md`
- `RELEASE_NOTES.md`, `CHANGELOG.md`, `docs/COMPATIBILITY.md`, this file, `CREDITS.md`, `LICENSE`
- `dist/` — prebuilt release binaries, driver package and SHA-256 checksums. It is intentionally tracked (see `.gitignore`); `dist/** -text` in `.gitattributes` preserves its CRLF files byte-for-byte.

Build scratch (`.build/`, `driver/out/`, `release/`, object files, screenshots, local backups), agent workspaces, signing material and local-only helper scripts are excluded from version control.
