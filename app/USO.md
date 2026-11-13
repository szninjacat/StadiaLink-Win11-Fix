# StadiaLink Rev 1.0.0 — user guide

StadiaLink Rev is an independent derivative of [minysito/StadiaLink](https://github.com/minysito/StadiaLink) (GPL-3.0). Its own version line starts at 1.0.0 and updates are checked only in this repository. Provenance and third-party code are recorded in [CREDITS.md](CREDITS.md) and [LICENSE](LICENSE).

Select a profile, adjust the controls and click **Save and apply**. Unsaved profile changes are discarded when you switch tabs.

- **Controller**: choose a button in the diagram and assign its function.
- **Sticks and triggers**: compare physical input with the configured output; adjust sensitivity, dead zone and inversion.
- **Vibration**: test each motor or enable continuous vibration. **Stop**, switching tabs or closing stops the test.
- **Profiles**: create, duplicate, import and export configurations.
- **Settings**: language, Windows startup, notifications, device and response measurement.

The Windows startup checkbox saves immediately. Keyboard shortcuts require StadiaLink Rev to stay open, including in the tray. Configure the same combination in your game or voice app for push to talk.

Profiles are stored in `%LOCALAPPDATA%/StadiaStudio/profiles.json`; this path is part of the app's storage layout. Uninstalling keeps profiles and lets you choose whether to remove the driver.

Vibration gain is applied and saved as you move the sliders; tests use the same gain as games. In **Settings** you can check for newer stable releases of this project (`szninjacat/StadiaLink-Rev`, 1.x version line) and open their downloads on GitHub. No other repository is queried. Downloads and installation are manual. The application-only update package preserves profiles and requires a compatible installed driver; otherwise use the full installer.
