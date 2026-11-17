# HT DSP

HT DSP is a standalone Windows 10/11 x64 audio processor with 16 independent mono channels. Select an ASIO or WASAPI device, then select CH 1–16 in the channel picker to edit its input and output routes. Channels can share an input. Each physical output can serve one DSP channel. The device and driver determine how many physical inputs and outputs are available.

Each channel has six parametric EQ bands, high- and low-pass filters, up to 500 ms of delay, gain, polarity, and output mute. Input and output meters have independent clip latches. The EQ graph overlays the measured input in cyan and output in pink. New sessions start with outputs muted until you unmute them or load a preset with saved mute states.

Use the phone URL shown in the Windows window for LAN control. HT DSP uses TCP port 8766 and requires its own PIN by default. Windows and each remote can select their channels independently. RTA calculations focus on the channels being viewed.

HT DSP saves 16-channel `.dsp.json` presets and can load a preset at launch. The app stores its settings under `RobbyApps/HTDSP` in the Windows user application-data directory. Its Start with Windows option runs it at sign-in. Simple DSP two-channel presets use a different channel count.

## Build

Use GitHub Actions and download the **HTDSP-Windows-x64** artifact once the Windows run passes. For a local build, run `build-windows.ps1` or `build-windows.bat` with CMake and Visual Studio 2022 installed.
