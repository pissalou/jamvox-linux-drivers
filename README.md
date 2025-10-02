# Jamvox USB Audio Driver for Linux

[![Build Status](https://github.com/pissalou/jamvox-driver/workflows/Build%20Jamvox%20Debian%20Package/badge.svg)](https://github.com/pissalou/jamvox-driver/actions)

Linux kernel driver for the VOX Jamvox USB audio interface.

## Features

- ✅ Playback and capture support
- ✅ 44.1kHz and 48kHz sample rates
- ✅ 16-bit and 24-bit audio depth
- ✅ DKMS support for automatic kernel updates
- ✅ Debian/Ubuntu package (.deb)

## Supported Devices

- VOX Jamvox USB Audio Interface (VID: 0x0944, PID: 0x0117)

## Installation

### From Release

1. Download the latest `.deb` from [Releases](https://github.com/pissalou/jamvox-driver/releases)
2. Install:
   ```bash
   sudo dpkg -i jamvox-driver-dkms_1.0.0-1_all.deb
   sudo apt-get install -f
   ```

### From Source

```bash
git clone https://github.com/pissalou/jamvox-driver.git
cd jamvox-driver
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../jamvox-driver-dkms_*.deb
```

## Usage

```bash
# Verify installation
cat /proc/asound/cards

# Test audio
speaker-test -c 2 -D hw:Jamvox

# Record test
arecord -D hw:Jamvox -f S16_LE -r 44100 -c 2 test.wav
```

## Requirements

- Ubuntu 22.04 or compatible
- Kernel 5.15+
- DKMS package

## License

GPL-2.0 or later
