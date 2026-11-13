# Dual Channel DSP — Windows prototype

Native Windows desktop app for **two independent mono paths**. No summing or cross-mixing:

```
Physical Input A  -> Gain -> 6 PEQ filters -> Delay -> Polarity -> Physical Output A
Physical Input B  -> Gain -> 6 PEQ filters -> Delay -> Polarity -> Physical Output B
```

Each path has its own independently selected **physical input** and **physical output** channel on the selected audio interface. Both channels use the same audio interface/driver and sample clock. Physical **output routes cannot be assigned to both paths**, preventing accidental summation. An input may be selected by both paths if deliberate input fan-out is wanted; the two outputs remain separate.

## Controls

- Per-channel input and output drop-downs, with `Unassigned` option
- Level: **-90 to +18 dB**, 0.1 dB precision
- **Six** independent peaking parametric EQ bands per channel, **20 Hz–20 kHz**, **-18 to +18 dB**, Q **0.1–15**
- Delay: **0–2,000 ms**, displayed in **0.01 ms** increments; fractional delay uses linear interpolation
- **Polarity invert** toggle (180° inversion, not variable phase rotation)
- Audio interface, driver type, sample rate and buffer size selections through JUCE's device selector
- ASIO enabled for Windows; WASAPI fallback

The DSP processing code uses no dynamic allocations while processing audio buffers. Filter settings are updated at buffer boundaries. Gain, polarity, and delay changes are smoothed to reduce switching artifacts. This is a **prototype**; there is no clip/limiter protection, EQ response plot, preset persistence, level metering, or installer in v0.1.

## No developer tools? Online Windows build

**You do not need Visual Studio on your PC.** The included `.github/workflows/build-windows.yml` runs on a GitHub-hosted Windows build machine and uploads a Windows `.exe`. See **`BUILD_WITHOUT_VISUAL_STUDIO.md`** for steps. This package is still **source code**, not a compiled binary; the Windows GUI compilation has not been verified.

## Build on Windows

Requirements: Windows 10/11, Visual Studio 2022 (Desktop development with C++ workload), CMake 3.22+, Git, internet access for the initial JUCE download. Install the **manufacturer's ASIO driver** for best multi-channel interface support.

1. Extract this directory.
2. Double-click `build-windows.bat` (or use `build-windows.ps1` from PowerShell).
3. The script generates a Visual Studio project, downloads **JUCE 9.0.5**, builds Release, runs the DSP unit tests, and lists the output `.exe` path.
4. Open the resulting `DualChannelDSP.exe` and select the **ASIO** device type and your interface. Set driver sample rate and buffer size. Select each path's input and output independently.

PowerShell can also execute the commands manually:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

## Signal-path / safety notes

- Start with level low, amp/speaker system muted, then gradually establish gain. There is **no output limiter** and six boosted PEQs can cause clipping.
- The app always clears all available output buffers at the start of each callback; only an output's assigned path writes to it. If a physical channel is unavailable or unassigned it is silent.
- Changing audio drivers or physical routing can briefly stop/restart the stream. Smooth DSP parameters are intended to be changed in real time.
- The delay control is **additional digital delay only**; actual end-to-end latency also includes interface/driver input/output buffers and hardware latency.
- Polarity inversion multiplies signal by -1; it is **not** a continuously variable all-pass phase adjustment.
- A live audio engine can be sensitive to Windows audio driver behavior. Before use with high-level PA equipment, validate the installed ASIO interface, route mapping, glitch handling, and signal levels.
- ASIO support uses bundled SDK headers in JUCE; check **JUCE and Steinberg ASIO licensing obligations** before distributing binaries.

## Implementation notes

- `Source/DSP.h` — DSP engine (six RBJ peaking biquads, ramping, fractional delay ring buffer).
- `Source/Main.cpp` — JUCE UI and real-time audio callback.
- `tests/dsp_test.cpp` — dependency-free DSP passthrough, polarity, and 10-ms delay tests.
- `CMakeLists.txt` — JUCE 9.0.5 FetchContent + CMake build.
- `build-windows.bat` — one-step Windows build and test.

**Build status:** Source and DSP tests can be prepared in this environment; a native Windows executable cannot be compiled or hardware-validated here. A Windows build and real-interface test are still necessary.
