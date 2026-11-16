# Simple DSP — Windows application v0.5.2

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

## v0.4 — Live meters and spectrum analyzer

- Independent **input (pre-DSP) and output (post-DSP)** peak/RMS meters for CH 1 and CH 2 on the Windows interface and LAN phone remote. The input meter remains active when the output is unassigned.
- A **4096-sample FFT RTA** from each channel's processed output, drawn in green beneath the cyan computed EQ response. The RTA has a separate -96 to 0 dBFS scale with 128 log-frequency bins.
- The Windows GUI updates at approximately 12 frames/sec; v0.5 raises phone `/api/telemetry` polling from 5 to 10 updates/sec. Audio metering uses atomics; FFT and JSON serialization do not execute inside the audio callback.
- No preset-format or routing changes. All existing settings, audio files, channel names, and startup presets remain compatible.

**RTA is digital spectrum level (dBFS), not calibrated acoustic SPL or a transfer function.**

## v0.5 — Dual RTA overlays and collapsible controls

- The EQ plot now overlays independent **pre-DSP input** (muted blue) and **post-DSP output** (green) 4096-sample FFT spectra, drawn beneath the computed cyan EQ response. The input trace is drawn first behind the output trace, and both use the same **-96 to 0 dBFS** scale.
- The **Filters** and **Parametric EQ** control sections are independently expandable per channel. Both are **collapsed by default** on Windows and the mobile remote. The EQ graph, input/output meters, level, delay, routing and polarity stay visible.
- The phone requests telemetry **every 100 ms (~10 times per second)** instead of 200 ms (~5 Hz). A request-in-flight guard prevents overlapping telemetry fetches on slow Wi-Fi. Actual display rate depends on network/phone rendering performance; no change to audio block size or processing latency.
- Reuses existing preset schema and legacy preset storage; collapse states and live spectra are presentation data and are not saved as preset parameters.
- Audio telemetry writes only atomic samples in the real-time callback. Both FFT analyses execute on the Windows GUI timer.

**The two traces are uncalibrated digital signal spectra, not a phase-coherent transfer function or acoustic measurement.** Input/output comparisons may also reflect delay, levels and any unrelated source variations.

## v0.5.1 — Consistent input/output monitoring colors

- **Input (pre-DSP): cyan** on the Windows and phone audio meters and input RTA spectrum.
- **Output (post-DSP): pink** on the Windows and phone audio meters and output RTA spectrum.
- **Computed EQ response: gold**, deliberately distinct from both measured traces. The underlying signal analysis and presets are unchanged.

## v0.5.2 — Windows text encoding repair

- Force **UTF-8 source and execution encoding** for the MSVC Windows GUI build (`/utf-8`). This prevents characters such as the collapse/expand arrows, list separators, degree sign and ellipsis from appearing as garbled symbols in the native app and embedded web page.
- Added a compile-time check to reject a future Windows build if the expected UTF-8 character encoding is missing.
- The processing engine, device routing, presets, saved data, and spectrum analysis are unchanged.

## Current channel processing

`Selected physical input → high-pass → 6 parametric EQs → low-pass → delay → level → polarity → selected physical output`

- Two entirely independent mono paths. **No summing or mixing.** Output assignments must be different.
- Each path has its own selectable input and output, name, output level (-90 to +18 dB), delay (0–500 ms, 0.01 ms steps), and polarity inversion.
- Six peaking parametric bands per channel (20 Hz–20 kHz, gain ±18 dB, Q 0.1–15), plus independently enabled 12/24 dB/oct high-pass and low-pass filters.
- Calculated EQ magnitude curves are plotted on desktop and on the mobile controller. The computed EQ curve is separate from the measured digital input/output RTA; neither constitutes an acoustic measurement or a phase-coherent transfer function.

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

- `Source/Analysis.h`: real-time-safe input/output metering and 4096-point spectrum analyzer
- `Source/DSP.h`: per-channel real-time processing and calculated EQ magnitude response
- `Source/Main.cpp`: Windows GUI, routes, presets, autostart, and phone-state sync
- `Source/RemoteServer.h`: local-only HTTP listener, optional PIN, and preset endpoints
- `Source/RemotePage.h`: embedded mobile control GUI (HTML/CSS/JavaScript)
- `assets/`: Windows ICO, image master, and mobile favicon/Home Screen PNGs
- `CMakeLists.txt`: JUCE compilation, application icon and embedded web assets
- `.github/workflows/build-windows.yml`: cloud Windows build, tests, downloadable EXE
- `tests/dsp_test.cpp`: DSP regression tests
