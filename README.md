# SimpleDSP

SimpleDSP is a standalone Windows 10/11 x64 audio processor with two independent mono channels. Select an ASIO or WASAPI device, then select CH 1 or CH 2 in the channel picker to edit its input and output routes. Channels can share an input. Each physical output can serve one DSP channel. The device and driver determine how many physical inputs and outputs are available.

Each channel has six parametric EQ bands, high- and low-pass filters, 0–50 ms of delay in 0.25 ms steps, gain, polarity, and output mute. Delay rounds to the nearest sample at the device rate. Live delay and filter changes use short crossfades. Input metering reads sample peak in dBFS. Output metering estimates true peak in dBTP with 4× interpolation. Both have independent, clearable overload indicators. Leave output headroom when boosting EQ or gain. The EQ graph overlays the measured input in cyan and output in pink. New sessions start with outputs muted until you unmute them or load a preset with saved mute states.

Use the phone URL shown in the Windows window for LAN control. SimpleDSP uses TCP port 8766 and requires its own PIN by default. Windows and each remote can select their channels independently. RTA calculations focus on the channels being viewed. On the phone, the two channel buttons sit above the meters and spectrum. Tap EQ, Input, or Output to show or hide a trace. Freeze RTA holds the spectrum while the meters continue to update.

SimpleDSP saves two-channel `.dsp.json` presets and can load a preset at launch. When loading a version 1 HT DSP preset, it uses channels 1 and 2 and rounds and clamps their delay values to the new range. The saved file retains all 16 channels until it is overwritten. Use a new preset name to keep the 16-channel original. The app stores its settings under `RobbyApps/HTDSP` in the Windows user application-data directory. Its Start with Windows option runs it at sign-in. Version 2 exports from the original two-channel SimpleDSP also load directly. Saved routes are applied when the selected audio device matches the one recorded in the preset.

## Build

Use GitHub Actions and download the **SimpleDSP-Windows-x64** artifact once the Windows run passes. Extract the ZIP and run `SimpleDSP.exe`. For a local build, run `build-windows.ps1` or `build-windows.bat` with CMake and Visual Studio 2022 installed.


## Test generator

The global generator offers pink noise, an adjustable 20 Hz–20 kHz sine oscillator, and a fixed 1 kHz tone. Global level ranges from -90 to 0 dBFS and defaults to -40 dBFS. The level is a peak ceiling. Pink noise has a lower average level than a sine at the same setting.

Each channel's Use test signal checkbox replaces its physical input before EQ, filters, delay, level and mute. Selected channels receive the same signal. When the generator is off, selected channels receive silence. Enabling the generator fades unchecked outputs to silence before starting playback. Disabling it fades the test signal out, allows up to 50 ms for DSP delay to drain, and fades live outputs back in. Unchecked channels stay muted during the test. Unchecking restores the physical input but keeps that output muted until the generator is switched off. The channel’s saved mute setting is preserved. Windows and the phone show TEST MUTED for these channels. Engaging a channel while the generator is on asks for confirmation on Windows and the phone.

Generator controls and channel assignments are session-only and start off when the app launches. Presets retain the existing DSP settings. Frequency is limited below Nyquist when the audio device uses a low sample rate.


## Sine sweep

Each channel has a Sine sweep button. Confirmation starts a 10-second logarithmic sweep from 20 Hz to 20 kHz, limited below Nyquist at lower sample rates, using the global generator level. The selected channel is temporarily unmuted and receives the sweep before its DSP. All other outputs are temporarily muted. Original channel mute settings are preserved. After completion or Stop sweep, normal generator isolation resumes if the global generator is on. Otherwise the original mute settings resume. A 0.6-second settling interval before and after the sweep clears the maximum 50 ms DSP delay and gives the filters time to settle. Stop sweep fades the tone before draining the delay. Only one sweep can run at a time. The global generator enable switch is independent of the sweep button.
