# Simple DSP — Windows application v0.3

**Simple DSP** is a locally running JUCE-based Windows audio processor with two separate mono processing paths today. The branding does not specify a channel count because more channels may be added in a future update.

**This download is the full source project, not a precompiled Windows EXE.** The included GitHub Actions workflow builds the EXE online, so Visual Studio is not required on your own PC. The GUI and live audio hardware still need Windows build and device validation.

## Updates in v0.3

- Renamed the app title, executable, workflow artifact and mobile remote to **Simple DSP**.
- Bundled the neon *Simple DSP* icon with the Windows application (`ICON_BIG` and `ICON_SMALL`) and the Wi-Fi page (favicon, Apple Home Screen icon, Android Home Screen icons and web app manifest). All are embedded in the EXE and served locally; no website host is required.
- Reduced the **delay maximum to 500 ms** everywhere: DSP engine, desktop slider, phone slider and preset imports. Older preset delays above this limit are clamped to 500 ms when loaded.
- Put **CH 1 / CH 2 next to editable names** on the Windows and mobile interfaces.
- Added a desktop **Require remote PIN** checkbox. Enabled by default to preserve older security behavior. You can disable it for a trusted local network; this choice survives restart. When disabled, a previously cached invalid phone PIN does not block access.
- Simplified the mobile page: no bottom explanatory text.
- Added **Set as load-at-launch** on the phone for a preset selected from the saved-presets list. The HTTP API waits for the Windows app to write and re-read its preference file before responding successfully. Both interfaces display the selected startup preset; the Windows footer also confirms the selection.
- Retained the legacy settings/presets folder and existing startup-file selection rather than erasing or moving them. Existing Windows startup registry settings migrate to the SimpleDSP Run entry.

## Current channel processing

`Selected physical input → high-pass → 6 parametric EQs → low-pass → delay → level → polarity → selected physical output`

- Two entirely independent mono paths. **No summing or mixing.** Output assignments must be different.
- Each path has its own selectable input and output, name, output level (-90 to +18 dB), delay (0–500 ms, 0.01 ms steps), and polarity inversion.
- Six peaking parametric bands per channel (20 Hz–20 kHz, gain ±18 dB, Q 0.1–15), plus independently enabled 12/24 dB/oct high-pass and low-pass filters.
- Calculated EQ magnitude curves are plotted on desktop and on the mobile controller. This is **not** an audio spectrum analyzer, real-time measurement or transfer-function tool.

## Presets: backward compatibility and confirmation

Presets remain readable `.dsp.json` files. The legacy path is **intentionally retained**:

`%APPDATA%\RobbyApps\DualChannelDSP\`

This preserves existing `settings.json`, `audio-device.xml`, and the `Presets` directory from the earlier Dual Channel DSP versions. The app name changes, but the storage location does not. Desktop presets saved to arbitrary other directories are also still loadable via **Load preset…**. Presets saved on the phone are in the `Presets` folder.

Old preset files preserve their channel names, six EQ bands, HP/LP filters, gain, polarity, and routing information; only delay values above 500 ms change on load. A saved routing arrangement is only applied when the selected audio device matches the device recorded in the preset. A preset must contain two channels for the current build. The previously configured startup preset remains selected and loads when **Load at launch** is enabled and the file exists.

**Desktop:** Choose **Startup file…** to select a saved `.dsp.json`. The desktop immediately displays `Load at launch: <filename> (selected and saved)` only after preferences were written and checked. Disable/enable load-at-launch using its checkbox.

**Phone:** Select an entry from **Saved presets**, then tap **Set as load-at-launch**. The button responds only after the Windows application confirms persistence; both the phone status and desktop selection update. This does *not* itself load that preset immediately. To load it right now, separately select **Load preset**.

## Phone remote, icons and PIN

Start the Windows app, then connect your phone to the **same private LAN/Wi-Fi** and open the displayed `http://<local-Windows-IP>:8765` URL. The remote webpage is hosted inside the application. Windows Firewall may prompt you: allow only **Private** networks. Do **not** forward TCP 8765 to the Internet.

On iPhone, open the URL in Safari, choose **Share → Add to Home Screen**. On Android, use Chrome's **Add to Home screen / Install app** menu. The Home Screen icon and browser favicon use the same **Simple DSP** icon as the Windows app. If a previous Home Screen shortcut shows an older icon, remove it and add it again so the device refreshes cached artwork.

The remote uses **unencrypted HTTP**; without a PIN, anybody on the reachable network can control the processing parameters. The PIN is optional because this is intended for trusted local Wi-Fi only. There is no cloud connection, remote public server, or audio streaming to the phone.

## Windows launch at sign-in

The **Start with Windows** checkbox creates a per-user Windows Run entry pointing at the current EXE. It starts after you sign into Windows, **not** before login. Move the executable to its permanent location first. An earlier `DualChannelDSP` registry entry is migrated to `SimpleDSP` when possible.

## GitHub build without Visual Studio

1. Extract the ZIP. Upload all its contents to the **root** of your existing SimpleDSP GitHub repo, preserving `Source/`, `tests/`, `assets/` and the hidden `.github/workflows/` directory. Replace older files. Do not upload a ZIP or an extra parent folder.
2. Go to GitHub → **Actions → Build Windows application**. A push to `main`/`master` should trigger the workflow, or use **Run workflow**.
3. If the run succeeds, download the **SimpleDSP-Windows-x64** artifact. Extract it and run `Simple DSP.exe` on Windows 10/11 (x64). No Visual Studio installation is needed on that computer.

If GitHub reports an error, open the failed run and inspect the first red build step; this project cannot be Windows-built in the current Linux environment.

## Tests and safety

The dependency-free DSP tests can be run with `g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test && ./dsp_test`. They cover unity processing, polarity inversion, delay, delay ceiling, high/low-pass slopes, filter response, and PEQ magnitudes. The mobile JavaScript can be checked with `node --check` after extracting the script from `Source/RemotePage.h`.

**Prototype limitations:** Windows UI, ASIO/WASAPI routing, and hardware audio have not been verified in this environment. Always begin with amps muted, level low, and a test signal. There is no output limiter, clipping protection, feedback safeguard, or measured system latency guarantee. Consult JUCE and ASIO licensing if redistributing a binary.

## Project contents

- `Source/DSP.h`: per-channel real-time processing and calculated EQ magnitude response
- `Source/Main.cpp`: Windows GUI, routes, presets, autostart, and phone-state sync
- `Source/RemoteServer.h`: local-only HTTP listener, optional PIN, and preset endpoints
- `Source/RemotePage.h`: embedded mobile control GUI (HTML/CSS/JavaScript)
- `assets/`: Windows ICO, image master, and mobile favicon/Home Screen PNGs
- `CMakeLists.txt`: JUCE compilation, application icon and embedded web assets
- `.github/workflows/build-windows.yml`: cloud Windows build, tests, downloadable EXE
- `tests/dsp_test.cpp`: DSP regression tests
