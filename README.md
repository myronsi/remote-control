# Remote Control

Private LAN remote-control application for Android → Windows/Linux PCs.

## Current target

- Android phone as the controller
- Linux PC agent first
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

The first input backend targets X11 using XTest. Wayland support is intentionally isolated behind the same platform interface and will be added separately because Wayland compositors restrict synthetic global input.

## Build the Linux agent

On Debian/Ubuntu:

```bash
sudo apt update
sudo apt install build-essential cmake libx11-dev libxtst-dev

cd agent
cmake -S . -B build
cmake --build build -j
./build/remote-control-agent
```

The current agent listens on TCP port `48721` and accepts newline-delimited JSON commands. This is a development protocol; authentication/encryption and discovery are planned before a production release.

## Development status

- [x] Repository skeleton
- [x] C++ agent build
- [x] TCP command server
- [x] Linux/X11 mouse movement
- [x] Linux/X11 mouse buttons
- [x] Linux/X11 scrolling
- [ ] Android touchpad UI
- [ ] LAN discovery
- [ ] Pairing and authentication
- [ ] Encryption
- [ ] Keyboard input
- [ ] Power/media controls
- [ ] Wayland backend
- [ ] Windows backend
- [ ] Packaging
