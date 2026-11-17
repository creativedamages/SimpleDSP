# HT DSP

HT DSP is a standalone Windows 10/11 x64 audio processor with 16 independent mono channels. Select an ASIO or WASAPI device, then select CH 1–16 in the channel picker to edit its input and output routes. Channels can share an input. Each physical output can serve one DSP channel. The device and driver determine how many physical inputs and outputs are available.

Each channel has six parametric EQ bands, high- and low-pass filters, up to 500 ms of delay, gain, polarity, and output mute. Input and output meters have independent clip latches. The EQ graph overlays the measured input in cyan and output in pink. New sessions start with outputs muted until you unmute them or load a preset with saved mute states.

Use the phone URL shown in the Windows window for LAN control. HT DSP uses TCP port 8766 and requires its own PIN by default. Windows and each remote can select their channels independently. RTA calculations focus on the channels being viewed.

HT DSP saves 16-channel `.dsp.json` presets and can load a preset at launch. The app stores its settings under `RobbyApps/HTDSP` in the Windows user application-data directory. Its Start with Windows option runs it at sign-in. Simple DSP two-channel presets use a different channel count.

## Build

Use GitHub Actions and download the **HTDSP-Windows-x64** artifact once the Windows run passes. For a local build, run `build-windows.ps1` or `build-windows.bat` with CMake and Visual Studio 2022 installed.


## Test generator

The global generator offers pink noise, an adjustable 20 Hz–20 kHz sine oscillator, and a fixed 1 kHz tone. Global level ranges from -90 to 0 dBFS and defaults to -40 dBFS. The level is a peak ceiling. Pink noise has a lower average level than a sine at the same setting.

Each channel's Use test signal checkbox replaces its physical input before EQ, filters, delay, level and mute. Selected channels receive the same signal. When the generator is off, selected channels receive silence. While the global generator is on, every unchecked channel is muted at its output, including any delayed audio. Unchecking restores the physical input but keeps that output muted until the generator is switched off. The channel’s saved mute setting is preserved. Windows and the phone show TEST MUTED for these channels. Engaging a channel while the generator is on asks for confirmation on Windows and the phone.

Generator controls and channel assignments are session-only and start off when the app launches. Presets retain the existing DSP settings. Frequency is limited below Nyquist when the audio device uses a low sample rate.


## Sine sweep

Each channel has a Sine sweep button. Confirmation starts a 10-second logarithmic sweep from 20 Hz to 20 kHz, limited below Nyquist at lower sample rates, using the global generator level. The selected channel is temporarily unmuted and receives the sweep before its DSP. All other outputs are temporarily muted. Original channel mute settings are preserved. After completion or Stop sweep, normal generator isolation resumes if the global generator is on. Otherwise the original mute settings resume. A brief settling interval before and after the sweep clears the maximum 500 ms DSP delay. Only one sweep can run at a time. The global generator enable switch is independent of the sweep button.
