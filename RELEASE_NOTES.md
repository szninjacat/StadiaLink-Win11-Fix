# StadiaLink Rev 1.0.0 — release notes

- **Date**: 2026-11-14
- **Version**: `1.0.0` (assembly, file and manifest versions `1.0.0.0`; release tag `v1.0.0`)
- **Platform**: Windows 11 x64, .NET Framework 4.8, UMDF 2.31
- **License**: GPL-3.0, see [LICENSE](LICENSE); third-party attribution in [CREDITS.md](CREDITS.md)
- **Provenance**: independent derivative of [minysito/StadiaLink](https://github.com/minysito/StadiaLink) (GPL-3.0); own 1.0 version line and an update channel restricted to this repository. See [CREDITS.md](CREDITS.md).
- **Documentation**: [README.md](README.md) (English, default), [README.zh-CN.md](README.zh-CN.md), [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md), [docs/CHANGES.md](docs/CHANGES.md)

StadiaLink Rev is an independent configuration tool for the Google Stadia controller on Windows. Version 1.0.0 is the first release on this project's own version line. It solves three practical problems — Bluetooth rumble failing on Windows 11 23H2, a UI that did not offer Simplified Chinese, and a close button that exited the app instead of leaving it in the tray — and adds a compatibility-first strategy for Windows builds that have not been verified yet.

---

## 1. Bluetooth rumble: cause and fix

Bluetooth rumble uses an undocumented private WinRT path: `GattClientDevice` → HID service → Report Reference → output report write.

| Item | Value |
| --- | --- |
| Activation factory statics IID (same on 22631 and 26200) | `CBAA1A5F-DCEF-508B-ACC6-158F1E160C0F` |
| Device interface IID (build 26200) | `6BBEDE1F-797F-5C17-8AC7-3F342037CB59` |
| Device interface IID (build 22631 / 23H2) | `0799A6A6-E942-5218-8A0E-7C9B996E4158` |
| Failure point | `created->QueryInterface()` → `E_NOINTERFACE` = `0x80004002` |

On 22631, device enumeration, INF matching and input reports were normal. The 26200 IID does not occur in any 22631 Bluetooth component binary, while `HidOverGatt.dll` places `0799A6A6…` 16 bytes after the shared statics IID. A temporary 16-byte binary patch — with no change to vtable slots, structure layout or async conventions — immediately restored rumble (`probeStatus=0x00000000`, `writes=10`, `stops=5`, `writeHRESULT=0x00000000`). This confirmed that the layouts match and the root cause is a single IID mismatch. The temporary patch is not part of this repository.

`driver/private-gatt.cpp` now holds both verified IIDs and tries them in order. On builds where neither is valid, it discovers candidates through `IInspectable::GetIids` and validates them as described in section 2. USB rumble, Bluetooth input, XInput translation, configuration, battery handling, signing, installation and INF device matching are unchanged.

## 2. Compatibility-first interface adaptation

An unknown build no longer disables rumble just because no IID is hardcoded. Candidates must pass every gate below before any vibration data is written.

| Gate | Mechanism | What it blocks |
| --- | --- | --- |
| Filtering | `driver/iid-policy.h`: version-5 parameterized interface, valid RFC 4122 variant bits, exclusion of standard WinRT interfaces, the shared statics IID and both verified IIDs; covered by 26 assertions in `tests/iid-policy-tests.cpp` | Mistaking a plumbing interface for a device interface |
| Static vtable check | Read-only: slots 6–14 must point into committed executable memory with at least five distinct implementations; candidates are ranked by methods sharing the activation factory's image | Placeholder interfaces, garbage pointers, unrelated interfaces |
| Semantic validation | Reuse the full `Open()` chain: exactly one HID service (`0x1812`), matching service UUID, Report Reference `5/2` | Different vtable slot semantics (`GetIids` cannot detect method reordering) |
| Call isolation | Unknown and cached interfaces are invoked inside `GuardedTryOpen()` under SEH; access violations become HRESULTs instead of crashing WUDFHost.exe | Host process crashes from a wrong guess |
| Persistence and circuit breaker | Success is cached in `Verified`; failures or crashes go to `Blacklist` (max 4); an `Attempt` marker left behind by a host crash blacklists that IID on the next boot. At most 3 candidates per boot | Crash loops and repeated probing on every boot |

Resolution order: verified IIDs → cached IID → runtime candidates. When a verified IID matches (22631 / 26200), the original code path runs unchanged — no SEH, registry access or policy decision is involved. When the registry is unavailable, persistence is silently skipped. If all candidates fail, stage `12` disables Bluetooth rumble and `driver/status.ps1` explains the next step. Build with `STADIALINK_AUTO_IID=0` for the strict fail-closed variant (stage `11`).

**Honest limits**: adaptation handles a changed interface IID, not a changed vtable method order. SEH catches most access violations but cannot guarantee in-process state, which is why `Attempt` and `Blacklist` exist. The adaptive path does not trigger on 22631 / 26200 and has no real-machine coverage; it is backed by compilation, 26 policy assertions, regression tests of the verified path and code review. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) for the matrix, diagnostic bytes, cache reset and the procedure for adding a verified IID.

## 3. Diagnostics

`driver/private-gatt.h` defines the stage enumeration; `driver/winstadia.c` writes diagnostics into bytes 60–63 of the status feature report, which were previously unused. Report length and all existing field offsets are unchanged. `driver/status.ps1` prints readable stage names, the selected IID source, candidate counts and async failure points.

| Byte | Meaning |
| --- | --- |
| 60 | Stage: `5` = device QI, `11` = no verified IID and adaptation disabled, `12` = all candidates failed, `127` = ready |
| 61 | IidIndex: `1` = 26200, `2` = 22631, `0x40` = cached, `0x80+i` = validated this run |
| 62 | CandidateCount: low 6 bits = count; `0x40` = cached IID tried; `0x80` = a previous host crash blacklisted an IID |
| 63 | AsyncStage: `Await()` failure point — `1` QI(IAsyncInfo), `2` get_Status, `3` GetResults, `4` async error, `5` timeout |

`tests/probe-private-gatt.py` is a read-only tool that enumerates private interface IIDs on a new Windows build; it never connects to a controller or writes a GATT characteristic.

## 4. Application changes

| File | Change |
| --- | --- |
| `app/assets/locales.tsv` | Five-column catalog (Simplified Chinese added), 289 entries, no empty values |
| `app/I18n.cs` | `zh / 简体中文` added and used as the default and fallback language |
| `app/MainWindow.cs` | **×** and Alt+F4 hide the window and remove it from the taskbar; `ForceClose()` is used by the tray **Exit** item and automated tests. Side bar and About show the assembly version with the StadiaLink Rev label |
| `app/App.cs` | Thirteen test self-exit points call `ForceClose()` so the new closing handler cannot hang them |
| `app/UpdateChecker.cs` | Checks only `szninjacat/StadiaLink-Rev`; validates the page URL, API URL, HTTPS, non-draft/non-prerelease and a plain `v`-prefixed version tag before comparing versions. `Verify()` asserts that foreign repositories, pre-release tags, malformed tags and drafts are rejected |
| `app/InstallerActions.cs` | Uninstall registry entry: `StadiaLink Rev`, dynamic `DisplayVersion`, publisher set to this repository |
| `app/App.cs`, `app/Setup.cs`, `app/app.manifest`, `app/package.ps1` | Version `1.0.0.0` and source package name `StadiaLink-Rev-1.0.0-source.zip` |
| `app/USO.md`, `README*.md`, `RELEASE_NOTES.md`, `docs/` | English documentation; Simplified Chinese, Spanish, French and German translations kept in the language READMEs |

The update notification marker is `StadiaLinkRev.UpdateNotified`, isolated from older builds. The app checks at startup and every six hours, but downloads and installation are always manual.

## 5. Build artifacts (`dist/`)

| File | Size (B) | SHA-256 (first 16) | Description |
| --- | --- | --- | --- |
| `StadiaLink.Setup.exe` | 607,744 | `d8b683a684f875d6` | Full installer: application, driver, native library and embedded source |
| `StadiaLink.exe` | 252,928 | `0f91d6e51dee416f` | WPF application x64 (version resource 1.0.0.0) |
| `StadiaLink.Update.exe` | 525,824 | `7cb81a91224a35ed` | Application-only updater |
| `StadiaDevice.dll` | 165,376 | `b0fa5952814f4adc` | Native battery reader |
| `driver/winstadia.dll` | 152,064 | `06b70008929b2cb7` | UMDF driver (verified IIDs, diagnostics, layered adaptation) |
| `driver/winstadia.inf` | 3,970 | `fc0a1d07216dbd48` | Driver INF (fixed VID_18D1&PID_9400 match) |

Three executables and `StadiaDevice.dll` were rebuilt for 1.0.0; the driver DLL and INF are unchanged from the verified build. Two `.exe.config` files and both checksum manifests ship alongside them.

The installer embeds `payload.zip` (application, native library, driver package, license, credits and full source). Its copies of `StadiaLink.exe`, `driver/pkg/winstadia.dll` and `driver/pkg/winstadia.inf` were extracted and compared byte-for-byte with `dist/`. Complete hashes are in `dist/SHA256SUMS.txt`; verify with `Get-FileHash -Algorithm SHA256`.

## 6. Build environment

- Visual Studio 2022 Build Tools 17.14.44 → MSVC **14.44.35207**
- Windows SDK **10.0.26100.0**, WDK NuGet **10.0.26100.6584** (UMDF 2.31)
- .NET Framework **4.8.1** (system `csc.exe` plus WPF reference assemblies)

Driver compilation produces only three pre-existing WDK header warnings and no errors. Rebuild from the default path:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy RemoteSigned
.\app\package.ps1 -Driver
.\tests\run.ps1
```

Optional switches: `STADIALINK_VC_ENV_READY=1` skips `vcvars64.bat` when the caller already supplies `INCLUDE`/`LIB`/`PATH`; `STADIALINK_SKIP_STAGE_CLEANUP=1` skips staging cleanup on hosts that block `Remove-Item`. Both default to off.

## 7. Verification

- `driver/out/pkg/winstadia.dll` contains both verified device IIDs, layered adaptation, SEH isolation and the registry circuit breaker; the copy embedded in the installer is identical.
- `tests/run.ps1`: **4,864,527** translation checks PASS, 26 IID policy assertions PASS, application model tests PASS.
- Application `--self-test` PASS (five-column catalog complete, dynamic translations work); installer `/languages-test` PASS for all five languages.
- The default-language screenshot and an explicitly `zh`-configured screenshot have identical SHA-256 hashes: the whole UI is Simplified Chinese, with only proper nouns left in Latin script.
- All three executables are valid x64 .NET assemblies with version resource `1.0.0.0`; `StadiaDevice.dll` is a native x64 PE. Every entry in the checksum manifest matches the shipped file.
- Real-machine rumble on 22631: `probeStatus=0x00000000`, `report=5 type=2 value=0x0044`, `writeHRESULT=0x00000000`, `writes` increasing.
- Close-behaviour self-test (`--draft-test`) PASS: minimize, **×** and system close all leave the app in the tray, and tray restore works.

## 8. Installation, upgrade and rollback

1. Exit any running StadiaLink Rev instance from the tray menu first (the single-instance lock blocks installation; **×** only minimizes).
2. Run `dist/StadiaLink.Setup.exe` and approve the administrator prompt for driver installation.
3. If the controller does not appear, toggle Bluetooth or reconnect it. `pnputil` returning **3010** means the device needs to be reconnected.
4. Check `driver/status.ps1`: success is `probeStatus=0x00000000`, `report=5 type=2 value=0x0044`, increasing `writes`, plus the private GATT stage and selected IID.
5. Rollback: `StadiaLink.Setup.exe /uninstall`, or run `driver/install.ps1 -Uninstall` as administrator.

Upgrading from an older build requires running this version's full `StadiaLink.Setup.exe` once: older executables contain a hardcoded update address that cannot be changed remotely. After installation the UI shows `StadiaLink Rev 1.0.0`, and only a newer stable release of this repository can produce an update notification.

## 9. Known limits

1. Bluetooth rumble depends on an undocumented private ABI. The statics IID was verified identical on 22631 and 26200; unknown builds are handled by the validated adaptation path in section 2, which does **not** adapt a changed vtable method order. Windows 10 is installable at the UMDF framework level only — the private GATT interface has no real-machine data there and is not guaranteed. See [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).
2. The private `GattClientDevice` class itself may be removed or restructured by a system update.
3. Platform: x64 only, UMDF 2.31 (Windows 10 1809+ / Windows 11), .NET Framework 4.8.1, INF fixed match `VID_18D1&PID_9400`.
4. The driver is **self-signed**, not Microsoft-signed. Certificate trust must be rebuilt on each machine, and SmartScreen / Secure Boot policies may block it.

## 10. Credits and license

The native UMDF driver is derived from [aisk/WinStadia](https://github.com/aisk/WinStadia) (reference commit `e9e84a0a24c3cc13bf7cc7ac2b8baa2b653ce80a`) and is included with modifications. Original copyright notices are retained; StadiaLink Rev and its modifications are distributed under **GPL-3.0**. See [CREDITS.md](CREDITS.md) and [LICENSE](LICENSE). This project is independent of Google and Microsoft.
