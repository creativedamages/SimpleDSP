# Simple DSP v0.4 — live monitoring update

Upload the **contents** of this ZIP to the root of the existing `creativedamages/SimpleDSP` GitHub repository. Replace identically named files, preserving `Source/`, `tests/`, and `.github/workflows/` paths. Do not upload an enclosing folder.

## Added

- Per-channel input (pre-DSP) and output (post-DSP) peak/RMS meters, both on Windows and the phone remote.
- 4096-sample FFT RTA (post-DSP output only), green and translucent behind the cyan EQ curve on Windows and mobile.
- `/api/telemetry` endpoint sends 128 log-frequency RTA bins + meter numbers independently of the preset/state endpoint.
- Spectrum work is performed on the Windows GUI timer, *not* in the audio callback.
- Separate analysis unit tests; GitHub Actions' existing CTest step will run the new test automatically.

## Kept unchanged

- Per-channel inputs/outputs; no summing or shared output routing.
- Six PEQ bands, HPF, LPF, gain, polarity, delay max 500 ms.
- Optional PIN, Wi-Fi remote icon and home-screen favicon.
- All preset file formats, storage locations, existing saved configurations, startup preferences, and launch-at-login setting.

## Build

GitHub Actions -> Build Windows application -> latest successful run -> `SimpleDSP-Windows-x64` artifact.

> The DSP and analysis tests ran on Linux and the remote controller JS passed syntax validation. The JUCE Windows GUI has **not** been compiled or hardware-tested in this environment. Start with outputs muted; there is no limiter.