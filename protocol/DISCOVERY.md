# LAN Discovery

The agent advertises itself on the local network using mDNS/DNS-SD.

## Service

```text
_remotecontrol._tcp
```

The service instance name is the human-readable PC name. The TXT record contains:

```text
version=1
protocol=1
```

The Android client discovers these services and resolves the hostname/port. The first implementation may fall back to the configured port `48721` for development.

## Security

Discovery is not authentication. A discovered device must still complete pairing/authentication before control commands are accepted.
