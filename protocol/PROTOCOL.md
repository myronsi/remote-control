# Remote Control Protocol v1

Development protocol used by the first Linux agent.

## Transport

- TCP
- Port: `48721`
- One UTF-8 JSON object per line
- Server replies with one JSON object per line

This protocol is intentionally simple for early development. Do not expose the agent outside the trusted LAN. TLS, pairing, replay protection and authorization are required before production use.

## Commands

### Hello

```json
{"version":1,"type":"hello"}
```

Response:

```json
{"version":1,"type":"hello_ack","agent":"remote-control-agent","protocol":1}
```

### Ping

```json
{"version":1,"type":"ping"}
```

Response:

```json
{"version":1,"type":"pong"}
```

### Relative mouse movement

```json
{"version":1,"type":"mouse_move","dx":12,"dy":-5}
```

### Mouse button

```json
{"version":1,"type":"mouse_button","button":"left","action":"down"}
```

Supported buttons: `left`, `middle`, `right`.

Supported actions: `down`, `up`.

### Scroll

```json
{"version":1,"type":"mouse_scroll","dx":0,"dy":-3}
```

Positive/negative values are interpreted by the platform backend and may be inverted by a future client setting.

## Errors

```json
{"version":1,"type":"error","message":"..."}
```
