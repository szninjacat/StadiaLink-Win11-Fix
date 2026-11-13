# Compatibility matrix and device-interface adaptation

This document is the reference for deciding whether Bluetooth rumble works on a given Windows build. **The driver being installable** (UMDF 2.31, x64, INF device match) and **the private GATT rumble interface working** are two separate things; the former does not imply the latter.

## 1. Compatibility matrix

| Windows version | Build | Bluetooth input | Bluetooth rumble | Device interface IID | Status |
| --- | --- | --- | --- | --- | --- |
| Windows 10 22H2 | 19045 | Untested | Untested | Unknown (auto-adaptation) | Unverified |
| Windows 11 22H2 | 22621 | Untested | Untested | Unknown (auto-adaptation) | Unverified |
| Windows 11 23H2 | 22631 | Verified | Verified | `0799A6A6-E942-5218-8A0E-7C9B996E4158` | Pass |
| Windows 11 24H2 | 26100 | Untested | Untested | Unknown (auto-adaptation) | Unverified |
| Windows 11 | 26200 | Verified | Verified | `6BBEDE1F-797F-5C17-8AC7-3F342037CB59` | Pass |

- **Verified** = real-machine install, input and rumble writes, with `driver/status.ps1` showing `probeStatus=0`, stage `127` and increasing `writes`.
- **Unverified** = no real-machine test. The driver attempts the adaptation in section 2 without guaranteeing success; `driver/status.ps1` shows whether the device IID is `cached` or `auto-adapted`.
- **There is no measured IID data for Windows 10.** UMDF 2.31 (Windows 10 1809+) only means the framework is installable; it says nothing about whether the private `GattClientDevice` device interface exists or has the same layout. Bluetooth rumble on Windows 10 is therefore not promised. Adaptation will try, but the first result must be confirmed on a real machine.
- The shared statics IID `CBAA1A5F-DCEF-508B-ACC6-158F1E160C0F` was verified identical only on 22631 / 26200.

## 2. Layered adaptation on unknown builds (enabled by default)

Goal: do not disable rumble merely because no IID is hardcoded, and never treat "this IID looks like a parameterized interface" as a sufficient reason to call a vtable.

Resolution order:

1. The two hardcoded, real-machine-verified IIDs (26200, 22631). A hit takes the **original code path** with none of the new logic, so behaviour on these systems is unchanged.
2. The IID validated and cached on a previous run (`Verified` under `PrivateGatt`).
3. Runtime candidates enumerated by `IInspectable::GetIids`.

A candidate must pass all five gates before any rumble data is written:

| Gate | Mechanism | Blocks |
| --- | --- | --- |
| Filtering | `driver/iid-policy.h`: version-5 parameterized interface (`Data3` high nibble `5`), valid RFC 4122 variant bits, exclusion of IUnknown / IInspectable / IAgileObject / IStringable / IClosable, the shared statics IID and both verified IIDs; 26 assertions in `tests/iid-policy-tests.cpp` | Mistaking WinRT plumbing for a device interface |
| Static vtable check | Read-only: slots 6–14 must point to committed executable memory with at least 5 distinct implementations; ranked by methods sharing the activation factory's image | Placeholder interfaces, garbage pointers, unrelated interfaces |
| Semantic validation | Reuse the full `Open()` chain: exactly one HID service (`0x1812`), matching service UUID, Report Reference `5/2`. All must pass | Different vtable slot semantics (`GetIids` cannot detect method reordering) |
| Call isolation | Unknown/cached calls run inside `GuardedTryOpen()` under SEH; access violations become HRESULTs instead of crashing WUDFHost.exe | Host process crashes after a wrong guess |
| Persistence and breaker | Success → `Verified`; failure/crash → `Blacklist` (max 4); `Attempt` is written before each try, and a marker left by a host crash blacklists that IID on the next boot. At most 3 candidates per boot | Crash loops and repeated probing every boot |

Degradation:

- If the registry cannot be written, persistence is silently skipped; adaptation still works but loses cross-reboot breaker state.
- All candidates failing → stage `12`, Bluetooth rumble disabled until an IID is added as in section 4.
- For a strict policy, build with `$env:STADIALINK_AUTO_IID=0; .\driver\build.ps1`: candidates are only counted, stage `11`, no unverified interface is called.

Honest limits:

- Adaptation solves a changed interface IID, **not** a changed vtable method order. If a new system changes the layout, chain validation fails and rumble is disabled safely.
- SEH catches most access violations but cannot guarantee in-process state; `Attempt` and `Blacklist` exist for that reason.
- The adaptive path does **not** trigger on 22631 / 26200 (hardcoded IIDs hit first), so it has no real-machine coverage. Its correctness rests on compilation, 26 policy assertions, regression tests of the verified path and code review. Confirm once on a real machine when first used on a new build (section 4).

## 3. Status report diagnostic bytes

| Byte | Meaning |
| --- | --- |
| 60 | Stage: `5` = device QI, `11` = no verified IID and adaptation unavailable, `12` = all candidates failed, `127` = ready |
| 61 | IidIndex: `1` = 26200, `2` = 22631, `0x40` = cached IID, `0x80+i` = validated this run (`i` = GetIids index) |
| 62 | CandidateCount: low 6 bits = count (max 63); `0x40` = cached IID tried; `0x80` = previous host crash, IID blacklisted |
| 63 | AsyncStage: `Await()` failure point — `1` QI(IAsyncInfo), `2` get_Status, `3` GetResults, `4` async error, `5` timeout |

Bytes 60–63 were previously unused; report length and existing field layout are unchanged. `driver/status.ps1` decodes them and gives next-step guidance for stages `11` / `12`.

## 4. Adding a hardcoded IID after adaptation fails

Only needed when stages `11` / `12` appear, or when you want to pin a validated IID:

1. Run `python tests/probe-private-gatt.py` on the target system (read-only enumeration of the activation factory's IIDs; the device facet belongs to the object returned by `CreateAsync`, so `E_NOINTERFACE` for the device IID here is expected).
2. Read bytes 61/62 with `driver/status.ps1`. `IidIndex` = `0x80+i` means adaptation already succeeded — copy the corresponding IID from the `GetIids` output. If all zero, locate the device interface IID next to the shared statics IID in an inbox binary (for example `HidOverGatt.dll`) or public symbols. **Do not** guess from the `0x5xxx` trait alone.
3. Confirm that vtable slot 11 (HID service enumeration) and slot 14 (opening a service object) still have the same semantics. A layout change requires a new analysis, not just a new IID.
4. Add the IID to `DeviceIids[]` in `driver/private-gatt.cpp` and the exclusion table in `driver/iid-policy.h`; update this matrix, `RELEASE_NOTES.md` and `docs/CHANGES.md`.
5. Verify on a real machine after installation: input works, `driver/status.ps1` shows stage `127`, `writes` increases, `writeHRESULT=0x00000000`, and rumble is physically felt.
6. Re-release using the default build (no switches).

## 5. Resetting the cache and blacklist

An invalid cached IID is cleared automatically and rediscovered. To force a clean retry after a wrong blacklist entry, clear the UMDF host account (LocalService) state from an elevated PowerShell:

```powershell
Remove-ItemProperty -Path 'Registry::HKEY_USERS\S-1-5-19\SOFTWARE\StadiaLink\PrivateGatt' -Name Verified,Blacklist,Attempt -ErrorAction SilentlyContinue
```

The state lives under `HKEY_USERS\S-1-5-19` for the host's `HKCU`; deleting values from a normal user's `HKCU:` has no effect. After the reset, the driver repeats section 2 on the next connection.

## 6. Maintenance conventions

Keep driver ABI compatibility, UI/localization and unrelated feature fixes in separate commits so each can be reviewed and reverted independently. Record real-machine evidence whenever a build is added to the verified list.
