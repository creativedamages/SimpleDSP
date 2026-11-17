# Simple DSP Pi — Raspberry Pi 4 (64-bit, headless)

This is a **separate Linux ARM64 build** of Simple DSP. The Windows app is preserved. No Docker, Visual Studio, desktop environment or development tools are needed on the Pi.

The Pi version uses ALSA and the existing DSP/mobile remote for two independent mono input/output paths (no summing), six PEQs, HPF/LPF, delay, level, polarity, independent output mute, clip and peak/RMS meters, input/output RTA, and saved presets.

## Hardware

- Raspberry Pi 4 with **Raspberry Pi OS Lite 64-bit**, Bookworm or newer.
- Focusrite Scarlett Solo 3rd Gen on USB, connected to the Pi.
- Phone on the same local Wi-Fi or LAN as the Pi.
- Start with amps/speakers muted and levels low; **there is no output limiter**.
- Turn Scarlett **Direct Monitor OFF**, to prevent a parallel unprocessed signal at its outputs.

## Download and installation

1. Use Raspberry Pi Imager to flash **Raspberry Pi OS Lite (64-bit)**. Enable SSH and configure Wi-Fi and your user in Imager settings.
2. On GitHub, open **Actions → Build Raspberry Pi 4 application (ARM64)** and download the **SimpleDSP-RaspberryPi4-ARM64** artifact from a successful run. Extract the ZIP on your Mac/PC.
3. Transfer the contents (executable, installer, example JSON and README) to the Pi. For SSH from the extracted folder on macOS/Linux:

       scp -r ./ user@raspberrypi.local:~/simple-dsp-install/

   Replace 'user' with the Pi account name. In Windows, PowerShell SCP or SFTP works. Replace raspberrypi.local with the Pi IP if name resolution fails.
4. SSH into the Pi, then run:

       cd ~/simple-dsp-install
       chmod +x install-pi.sh
       ./install-pi.sh

   **Do not run the script with sudo.** It requests elevated access only when required. It installs ALSA runtime utilities, copies the executable, creates a remote PIN, and enables the systemd boot service.
5. See the phone URL and remote PIN:

       journalctl -u simple-dsp -n 60 --no-pager

   Open the private IP URL on your phone (port 8765). On iPhone use Safari → Share → Add to Home Screen.
6. Check Scarlett input and output routing and levels, then unmute from the phone. The app **starts muted on every reboot** unless you explicitly change that safeguard.

## Service controls

       sudo systemctl status simple-dsp --no-pager
       journalctl -u simple-dsp -f
       sudo systemctl restart simple-dsp
       sudo systemctl stop simple-dsp
       /opt/simple-dsp/simple-dsp-pi --list-devices
       /opt/simple-dsp/simple-dsp-pi --check-config --config ~/.config/simple-dsp/config.json
       aplay -l
       arecord -l

## Configuration

Path: ~/.config/simple-dsp/config.json

- audio_device: Case-insensitive ALSA device-name substring. An empty string selects only ALSA devices containing "Scarlett" or "Focusrite", never a random output.
- sample_rate: Default 48000. buffer_size: Default 256 samples (about 5.3 ms buffer at 48 kHz, not measured round-trip latency).
- remote_pin_required: True by default. Set false only on a trusted private LAN; the HTTP control connection is not encrypted.
- remote_pin: Six digits generated once on installation; leave existing config intact during upgrades.
- bind_address: Empty uses a private LAN IPv4 address. Public addresses and 0.0.0.0 are rejected.
- http_port: Default 8765; never forward this port to the Internet.
- start_muted: True by default, overrides unmuted state in a startup preset on boot. After verified testing, set false to allow unattended audio.
- load_at_launch/startup_preset: Updated when the phone selects "Set as load-at-launch."

After manually editing the JSON, restart the service. If audio doesn't open, stop the service and use the --list-devices command. Edit audio_device to match what ALSA reports, then restart. Raspberry Pi OS Lite avoids desktop audio servers that might hold an ALSA interface.

## Presets

Phone presets are stored under:

    ~/.local/share/simple-dsp/Presets/*.dsp.json

Copy existing Windows .dsp.json presets here to use them. EQ, filter, gain, delay, polarity, names and mute values are compatible. Hardware routes from Windows are **not copied to Pi** if the saved device name differs. The Pi retains its active safe routes.

The phone startup-preset selection saves to the Pi config file, and existing presets/configuration survive app upgrades. Back up these directories before re-flashing the SD card.

## Performance and verification

- The ARM64 artifact is built in a Debian Bookworm container with ARMv8-A code generation so it can run on Raspberry Pi OS Bookworm or newer. Requires 64-bit OS.
- An ALSA audio callback processes each path independently. The FFT runs outside the real-time thread at approximately 5 Hz, while meters and phone telemetry update at 10 Hz.
- The service automatically starts at boot and retries detection if Wi-Fi or the USB interface arrives late.
- The GitHub CI job builds ARM64 and tests DSP, FFT and command-line startup. Real Pi 4/Scarlett compatibility, audio latency and glitch-free performance **still require hardware validation**.

This is a prototype. It has no limiter, feedback protection, measured latency guarantee or phase-coherent acoustic transfer-function analyzer. Review JUCE licensing obligations before distributing software.
