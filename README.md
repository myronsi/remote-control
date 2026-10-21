# Remote Control

Private LAN remote-control application for Android → Windows/Linux PCs.

## Current target

- Android phone as the controller
- Linux PC agent first
- Ubuntu/Debian with GNOME/KDE
- Same WLAN only
- No cloud account or backend
- C++20 + CMake for the PC agent
- Kotlin + Jetpack Compose for Android
- Windows backend will be added after the Linux MVP

## Repository layout

```text
remote-control/
├── agent/       # C++ PC agent
├── android/     # Android application
├── protocol/    # Wire protocol specification
└── docs/        # Design notes
```

## Linux MVP

The first input backend targets X11 using XTest. Wayland support is isolated behind the same platform interface and will be added separately because Wayland compositors restrict synthetic global input.

The agent advertises itself on the local network using Avahi DNS-SD as `_remotecontrol._tcp`. Android discovers this service using the native Android NSD API.

## Build the Linux agent

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libavahi-client-dev libx11-dev libxtst-dev

cd agent
cmake -S . -B build
cmake --build build -j
./build/remote-control-agent
```

Avahi's system service should also be running:

```bash
systemctl status avahi-daemon
```

The current agent listens on TCP port `48721` and advertises that port through DNS-SD. The TCP protocol is still a development protocol; authentication/encryption are required before production use.

## Android

Open the `android/` directory in Android Studio and run the `app` configuration on an Android device or emulator. The app discovers `_remotecontrol._tcp` services and displays resolved PC names, addresses and ports. Manual IP entry remains available as a development fallback.

## Development status

- [x] Repository skeleton
- [x] C++ agent build
- [x] TCP command server
- [x] Linux/X11 mouse movement
- [x] Linux/X11 mouse buttons
- [x] Linux/X11 scrolling
- [x] Android touchpad UI
- [x] LAN discovery protocol
- [x] Linux DNS-SD advertisement foundation
- [x] Android DNS-SD discovery
- [ ] Pairing and authentication
- [ ] Encryption
- [ ] Keyboard input
- [ ] Power/media controls
- [ ] Wayland backend
- [ ] Windows backend
- [ ] Packaging
