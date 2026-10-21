#!/usr/bin/env python3
"""Tiny development client for the v0.1 TCP protocol.

Usage:
    python3 agent/tests/test_client.py 127.0.0.1
"""

import json
import socket
import sys
import time


def send(sock, message):
    payload = (json.dumps(message) + "\n").encode()
    sock.sendall(payload)
    print("->", message)
    response = sock.recv(4096).decode().strip()
    print("<-", response)


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"

    with socket.create_connection((host, 48721), timeout=3) as sock:
        send(sock, {"version": 1, "type": "hello"})
        send(sock, {"version": 1, "type": "ping"})

        # Small movement and click smoke test.
        send(sock, {"version": 1, "type": "mouse_move", "dx": 20, "dy": 0})
        time.sleep(0.1)
        send(sock, {"version": 1, "type": "mouse_button", "button": "left", "action": "down"})
        send(sock, {"version": 1, "type": "mouse_button", "button": "left", "action": "up"})


if __name__ == "__main__":
    main()
