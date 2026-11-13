# Dual Channel DSP — Windows v0.2 source

This is the updated **two-channel Windows audio processor**. Each mono signal path is independent; there is **no summing, stereo mixing, or cross-bus**. Each path has its own selectable physical input and output on a common interface/audio clock. The app rejects duplicate output routes.

**This archive contains source code, not a precompiled EXE.** Use the bundled GitHub Actions workflow to build on a hosted Windows runner without installing Visual Studio. A successful Windows build and a real audio-interface hardware test have not yet been verified.

## Channel signal path

`Physical input -> high-pass -> six parametric EQ bands -> low-pass -> delay -> level -> polarity -> physical output`

Controls per channel:

- Editable channel name, saved in presets
- Physical input and output route, individually selectable (unassigned possible)
- Level: −90 to +18 dB, 0.1 dB steps
- Six peaking EQs: 20–20,000 Hz, ±18 dB, Q 0.1–15
- High-pass and low-pass, independently enabled, frequency 20–20,000 Hz, slope selectable 12 or 24 dB/oct (Butterworth biquads)
- Live combined EQ frequency response chart (desktop and phone, 20 Hz–20 kHz, ±24 dB vertical view)
- Delay: 0–2,000 milliseconds, 0.01 ms steps
- Polarity inversion (Ø, −1 multiplier; not variable phase rotation)

Frequency response graphs include HP/LP and all six PEQ bands. The graph excludes the overall level setting and delay. It shows calculated filter response, **not** an audio spectrum analyzer or a measured transfer function.

## Presets and startup

- **Save preset… / Load preset…:** save a readable `.dsp.json` file anywhere on your Windows PC, including both channel names, routes, gain, delay, polarity, and filters.
- **Startup file…:** choose the `.dsp.json` to load automatically, then enable **Load at launch**. These app preferences remain stored on the PC.
- **Start with Windows:** toggle autostart in the current user's Windows Run registry key. It starts **after that user signs in**, not as a pre-login system service. No admin rights are required in normal configurations. Keep the EXE in its permanent location before enabling the checkbox.
- The last selected audio interface and buffer/rate settings are saved for the next launch using JUCE's audio-device state XML.
- Preset routes are applied only when the current audio device matches the one used to save the preset. Otherwise routing is left unassigned to prevent accidentally driving the wrong physical outputs.

App settings are stored under `%APPDATA%\RobbyApps\DualChannelDSP\` and mobile-saved presets under the `Presets` subfolder.

## Mobile Wi-Fi remote

When the Windows app is running, it displays an address such as `http://192.168.1.25:8765` and a **6-digit PIN**. Connect your phone to the same local/private Wi-Fi, open that address in the mobile browser, and enter the displayed PIN. The page is served by the Windows app—**no cloud account, tunnel, or separate web hosting** is needed. The phone can change both channels' routes, names, level, delay, polarity, six EQs and HP/LP settings, see both EQ graphs, and save/load presets stored on the PC.

The app binds to a private IPv4 interface rather than all network adapters. If your PC has several private LAN/virtual adapters, the displayed address might correspond to the wrong one; disable unused adapters if necessary. Windows Firewall may ask permission: **allow Private networks**, not Public. Phone and PC must be able to reach each other (guest networks/client isolation will block it).

**Security limitation:** the remote uses HTTP with a PIN, not encrypted HTTPS. It is intended only for trusted private Wi-Fi networks; anyone with network access and the PIN can adjust the audio. Do not forward TCP port 8765 or expose it to the Internet.

## GitHub upload & no-Visual-Studio build

1. Extract the entire ZIP to a normal folder. `.github` is a hidden folder on macOS; Finder **Command+Shift+.** reveals it.
2. Update your GitHub repository with the files, preserving the exact folder structure, especially `.github/workflows/build-windows.yml`. The files in `Source/` and `tests/` must remain inside those folders. Do not add an extra enclosing directory.
3. Open GitHub → **Actions** → **Build Windows application**. It runs on pushes to `main` or `master`, and can also be started via **Run workflow**.
4. When the workflow succeeds, download the **Dual-Channel-DSP-Windows-x64** artifact. Extract the artifact and run `Dual Channel DSP.exe` on a 64-bit Windows 10/11 PC. No Visual Studio is needed on that PC. Install your interface's ASIO driver if using ASIO.
5. Move the executable to its intended permanent folder before enabling **Start with Windows**.

The workflow currently uses JUCE 9.0.5, Windows Server 2022 build runner, and `actions/checkout@v5`. `project(... LANGUAGES C CXX)` and non-`constexpr` JUCE color declarations avoid the earlier CMake/MSVC build failures.

## Local DSP tests

Pure C++ DSP regression tests can run anywhere with a C++17 compiler:

```bash
g++ -std=c++17 -O2 tests/dsp_test.cpp -o dsp_test
./dsp_test
```

They check pass-through, polarity, 10ms delay, high-pass/low-pass response for 12/24 dB slopes, and 6 dB PEQ magnitude. The **Windows GUI and audio device have not been compiled or hardware-tested in this environment**; the GitHub Actions build is the integration test.

## Safety and limitations

- **Start testing with amps muted and at low audio levels.** There is no output limiter, clip protection, feedback prevention, or signal metering; large EQ boosts can clip downstream equipment.
- Changing the audio driver, sample rate, buffer size, or physical channel masks can briefly interrupt audio. Gain, delay and polarity are ramped; filter coefficient changes are applied between audio buffers, without crossfading between EQ shapes.
- This is an audio processing **prototype**, not a certified loudspeaker DSP. Validate phase, latency, headroom, physical channel routing, glitches, and driver restart behavior before use in a live PA system.
- The remote is active only while the Windows app is running. Autostart is login-based and requires an interactive user session and functioning audio interface.
- The interface and remote pages are local. No user audio is streamed to the phone; the remote sends parameters only.
- If packaging or redistributing an ASIO-enabled build, review JUCE and Steinberg SDK licensing.

## Project files

- `Source/DSP.h`: allocation-free callback DSP, biquads, response model, fractional delay
- `Source/Main.cpp`: JUCE Windows GUI, desktop EQ graph, routing, JSON presets, preferences, Windows autostart
- `Source/RemoteServer.h`: local PIN-protected HTTP listener and preset endpoints
- `Source/RemotePage.h`: embedded phone-friendly HTML/CSS/JavaScript controller
- `tests/dsp_test.cpp`: standalone audio DSP regression tests
- `CMakeLists.txt`: JUCE project
- `.github/workflows/build-windows.yml`: hosted Windows build + EXE artifact
- `BUILD_WITHOUT_VISUAL_STUDIO.md`: original no-IDE build procedure
