# Linux Wayland input

The first Linux backend uses `/dev/uinput` to create a virtual mouse device. This works at the Linux input layer and is suitable for KDE Plasma Wayland; it does not depend on X11/XTest.

## Permissions

The agent needs permission to open `/dev/uinput`.

On Ubuntu/Debian, check:

```bash
ls -l /dev/uinput
```

If the device is owned by group `input`, add your user to that group:

```bash
sudo usermod -aG input "$USER"
```

Log out and back in afterward. Verify:

```bash
id -nG | tr ' ' '\n' | grep '^input$'
```

Then rebuild and run the agent.

## Why uinput

Wayland clients cannot use the old X11 `XTest` mechanism to inject arbitrary global pointer events. `/dev/uinput` creates a virtual kernel input device which the desktop compositor sees as an input device. The existing network protocol therefore remains independent of the desktop environment.

The current backend implements relative mouse movement, left/middle/right buttons, and vertical/horizontal wheel events.
