# Changelog

## 1.0.0 — initial independent release (2026-11-14)

Full notes: [RELEASE_NOTES.md](RELEASE_NOTES.md) · Implementation notes: [docs/CHANGES.md](docs/CHANGES.md) · Compatibility: [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

- First release under the **StadiaLink Rev** name and its own **1.0 version line**. The repository, documentation, installer metadata and UI labels were renamed accordingly; assembly, file and manifest versions are `1.0.0.0`.
- The update channel is restricted to stable, non-draft releases of `szninjacat/StadiaLink-Rev` with a plain `v`-prefixed version tag. Both the release page URL and the API URL are checked against this repository, so no external project can trigger an update prompt. Downloads remain manual.
- Fixed Bluetooth rumble failing with `E_NOINTERFACE` (`0x80004002`) on Windows 11 23H2 (build 22631): the private `GattClientDevice` device-interface IID differs from build 26200. The driver now tries both verified IIDs and falls back to `IInspectable::GetIids` on unknown builds.
- Added layered runtime adaptation for unknown builds: strict candidate filtering (`driver/iid-policy.h`, 26 unit assertions), non-executing vtable sanity checks, the full HID GATT chain as a semantic validator, an SEH guard and a persistent crash circuit breaker (`Verified` / `Blacklist` / `Attempt`). `STADIALINK_AUTO_IID=0` builds the strict fail-closed variant. Verified behaviour on 22631 / 26200 is unchanged.
- Added private-GATT open-stage and IID-selection diagnostics to the status feature report and `driver/status.ps1`.
- Added Simplified Chinese as the fifth UI language and made it the default (289 translated entries); fixed the close button so **×** and Alt+F4 minimize to the notification area while the tray **Exit** item ends the process.
- Provenance documented: StadiaLink Rev is an independent derivative of [minysito/StadiaLink](https://github.com/minysito/StadiaLink) (GPL-3.0). The version line restarts at 1.0.0 and the update channel never consults the original project; see [CREDITS.md](CREDITS.md).
- Documentation in English by default, with Simplified Chinese, Spanish, French and German translations. Third-party attribution is recorded in [CREDITS.md](CREDITS.md).
- Shipped rebuilt prebuilt binaries and SHA-256 checksums in `dist/`.
