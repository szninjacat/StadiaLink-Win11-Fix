# Changelog

## Unreleased — 1.0.2 hotfix (2026-11-09)

Full notes (简体中文): [RELEASE_NOTES.md](RELEASE_NOTES.md) · Per-file changes: [docs/CHANGES.md](docs/CHANGES.md) · Code diff: [patches/v1.0.2-hotfix.patch](patches/v1.0.2-hotfix.patch).

- Fixed Bluetooth rumble failing with `E_NOINTERFACE` (`0x80004002`) on Windows 11 23H2 (build 22631). The private `GattClientDevice` device-interface IID differs from build 26200; the driver now tries both known IIDs and falls back to `IInspectable::GetIids` on unknown builds.
- Added private-GATT open-stage and IID-selection diagnostics to the status feature report and `driver/status.ps1`.
- Added Simplified Chinese as the fifth UI language and made it the default. The full 289-entry catalog was translated and the delete-profile dialog punctuation was fixed.
- Added `README.zh-CN.md` with full Simplified Chinese documentation and cross-linked the language navigation in all five READMEs.
- Made Simplified Chinese the default repository README (`README.md`); kept the Spanish README as `README.es.md` and updated the language navigation in all six READMEs.
- Added opt-in build switches `STADIALINK_VC_ENV_READY` and `STADIALINK_SKIP_STAGE_CLEANUP` for hosts/CI that cannot run `vcvars64.bat` or remove staging directories. Default behaviour is unchanged.
- Fixed the window close button exiting the app: the × button and Alt+F4 now minimize to the notification area, while the tray exit item and automated tests terminate the process through `ForceClose()`. Draft-behaviour tests now cover the close button, system close and tray restore.
- Shipped rebuilt prebuilt binaries and SHA-256 checksums in `dist/` (including the close-button fix).
- Code-review hardening (2026-11-10): unverified device IIDs are no longer called automatically (fail-closed, stage 11, `STADIALINK_ALLOW_UNVERIFIED_IID=1` is a test-build-only opt-in); `QueryInterface` failures other than `E_NOINTERFACE` are propagated; `Await()` sub-stages and the unverified-candidate count are reported in status report bytes 62/63; `probe-private-gatt.py` now prints canonical GUIDs. Added [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md) with the compatibility matrix and ABI validation procedure, and corrected `cd StadiaLink` to `cd StadiaLink-Win11-Fix` in all six READMEs. Windows 10 Bluetooth rumble is explicitly marked unverified. Verified behaviour on builds 22631 / 26200 is unchanged.

## 1.0.2

- Improved vibration gain, live adjustments and full-range tests.
- Added slider track clicks, update notifications and an app-only updater.

## 1.0.1

- Fixed incomplete translations in English, French and German.

## 1.0.0

- Initial release.
