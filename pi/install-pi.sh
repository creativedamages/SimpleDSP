#!/usr/bin/env bash
set -euo pipefail

# Run as the intended Raspberry Pi user, NOT with sudo.
# Settings and presets remain in the user's HOME for future upgrades.
if [[ "$EUID" -eq 0 ]]; then
  echo "Run ./install-pi.sh as your normal Pi user (not with sudo)." >&2
  exit 1
fi
if [[ "$(uname -m)" != "aarch64" ]]; then
  echo "Requires Raspberry Pi OS 64-bit (aarch64). Use Raspberry Pi Imager." >&2
  exit 1
fi
DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ ! -f "$DIR/simple-dsp-pi" || ! -f "$DIR/config.example.json" ]]; then
  echo "Extract the entire SimpleDSP-RaspberryPi4-ARM64 artifact before running." >&2
  exit 1
fi

USER_NAME="$(id -un)"
if [[ ! "$USER_NAME" =~ ^[a-zA-Z0-9_-]+$ ]]; then
  echo "Unexpected username; installer stopped." >&2
  exit 1
fi
HOME_DIR="$HOME"
if [[ "$HOME_DIR" == *$'\n'* || "$HOME_DIR" == *' '* ]]; then
  echo "Home path with spaces/newlines is not supported by this installer." >&2
  exit 1
fi
if ! getent group audio >/dev/null 2>&1; then
  echo "ALSA 'audio' group not found; check Raspberry Pi OS installation." >&2
  exit 1
fi

echo "Installing Simple DSP Pi for $USER_NAME on Raspberry Pi OS..."
sudo apt-get update
sudo apt-get install -y --no-install-recommends libasound2 alsa-utils ca-certificates
sudo install -d -m 755 /opt/simple-dsp
sudo install -m 755 "$DIR/simple-dsp-pi" /opt/simple-dsp/simple-dsp-pi

CONFIG_DIR="$HOME_DIR/.config/simple-dsp"
STATE_DIR="$HOME_DIR/.local/share/simple-dsp"
mkdir -p "$CONFIG_DIR" "$STATE_DIR/Presets"
chmod 700 "$CONFIG_DIR" "$STATE_DIR"
if [[ ! -f "$CONFIG_DIR/config.json" ]]; then
  cp "$DIR/config.example.json" "$CONFIG_DIR/config.json"
  PIN="$(shuf -i 100000-999999 -n 1)"
  sed -i "s/\"remote_pin\": \"\"/\"remote_pin\": \"$PIN\"/" "$CONFIG_DIR/config.json"
  chmod 600 "$CONFIG_DIR/config.json"
  echo "Created default configuration; remote PIN: $PIN"
else
  echo "Existing config.json preserved."
fi

# systemd runs as this user, with audio-group access for ALSA USB devices.
sudo tee /etc/systemd/system/simple-dsp.service >/dev/null <<UNIT
[Unit]
Description=Simple DSP headless Raspberry Pi audio processor
Wants=network-online.target
After=network-online.target sound.target

[Service]
Type=simple
User=$USER_NAME
Group=$(id -gn)
SupplementaryGroups=audio
Environment=HOME=$HOME_DIR
WorkingDirectory=/opt/simple-dsp
ExecStart=/opt/simple-dsp/simple-dsp-pi --config $CONFIG_DIR/config.json
Restart=on-failure
RestartSec=5
NoNewPrivileges=true
ProtectSystem=full
PrivateTmp=true

[Install]
WantedBy=multi-user.target
UNIT

sudo systemctl daemon-reload
sudo systemctl enable --now simple-dsp.service
echo
echo "Service: sudo systemctl status simple-dsp --no-pager"
echo "Logs:    journalctl -u simple-dsp -n 50 --no-pager"
echo "Config:  $CONFIG_DIR/config.json"
echo "Presets: $STATE_DIR/Presets"
echo "The first boot is MUTED. Open the phone URL from the service logs and unmute only after checking output levels."
