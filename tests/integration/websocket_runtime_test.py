#!/usr/bin/env python3

import base64
import json
import os
import socket
import struct
import sys
import time
import urllib.error
import urllib.parse
import urllib.request


BASE = sys.argv[1].rstrip("/")
HOST = urllib.parse.urlparse(BASE).hostname
PORT = urllib.parse.urlparse(BASE).port or 80
RUN_ID = str(time.time_ns())


def http(method, path, body=None, token=None, expected=(200,)):
    headers = {"X-Request-ID": f"websocket-{RUN_ID}"}
    if body is not None:
        headers["Content-Type"] = "application/json"
    if token:
        headers["Authorization"] = f"Bearer {token}"
    request = urllib.request.Request(
        BASE + path,
        data=json.dumps(body).encode() if body is not None else None,
        headers=headers,
        method=method,
    )
    try:
        with urllib.request.urlopen(request, timeout=5) as response:
            status, payload = response.status, json.loads(response.read() or b"{}")
    except urllib.error.HTTPError as error:
        status, payload = error.code, json.loads(error.read() or b"{}")
    assert status in expected, (method, path, status, payload)
    return payload


class WebSocket:
    def __init__(self, token):
        self.socket = socket.create_connection((HOST, PORT), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        request = (
            f"GET /api/v1/ws HTTP/1.1\r\nHost: {HOST}:{PORT}\r\n"
            f"Upgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\nAuthorization: Bearer {token}\r\n\r\n"
        )
        self.socket.sendall(request.encode())
        response = b""
        while b"\r\n\r\n" not in response:
            response += self.socket.recv(4096)
        assert response.startswith(b"HTTP/1.1 101"), response

    def receive(self):
        first = self._read(2)
        opcode = first[0] & 0x0F
        length = first[1] & 0x7F
        if length == 126:
            length = struct.unpack("!H", self._read(2))[0]
        elif length == 127:
            length = struct.unpack("!Q", self._read(8))[0]
        payload = self._read(length)
        if opcode == 8:
            raise RuntimeError("websocket closed")
        return json.loads(payload)

    def send(self, payload):
        encoded = json.dumps(payload).encode()
        mask = os.urandom(4)
        header = bytearray([0x81])
        if len(encoded) < 126:
            header.append(0x80 | len(encoded))
        elif len(encoded) <= 65535:
            header.append(0x80 | 126)
            header.extend(struct.pack("!H", len(encoded)))
        else:
            header.append(0x80 | 127)
            header.extend(struct.pack("!Q", len(encoded)))
        masked = bytes(value ^ mask[index % 4] for index, value in enumerate(encoded))
        self.socket.sendall(bytes(header) + mask + masked)

    def _read(self, size):
        data = b""
        while len(data) < size:
            chunk = self.socket.recv(size - len(data))
            if not chunk:
                raise RuntimeError("unexpected websocket EOF")
            data += chunk
        return data


owner = http(
    "POST",
    "/api/v1/auth/register",
    {"email": f"ws-owner-{RUN_ID}@example.test", "password": "correct horse battery staple"},
    expected=(201,),
)
member = http(
    "POST",
    "/api/v1/auth/register",
    {"email": f"ws-member-{RUN_ID}@example.test", "password": "correct horse battery staple"},
    expected=(201,),
)
project = http(
    "POST",
    "/api/v1/projects",
    {"name": "WebSocket integration", "description": RUN_ID},
    owner["access_token"],
    (201,),
)
http(
    "POST",
    f"/api/v1/projects/{project['id']}/members",
    {"user_id": member["user"]["id"], "role": "member"},
    owner["access_token"],
    (201,),
)

websocket = WebSocket(member["access_token"])
assert websocket.receive()["kind"] == "ready"
task = http(
    "POST",
    "/api/v1/tasks",
    {"project_id": project["id"], "title": "Live task"},
    owner["access_token"],
    (201,),
)
http(
    "POST",
    f"/api/v1/tasks/{task['id']}/comments",
    {"body": "Live comment"},
    owner["access_token"],
    (201,),
)

highest = 0
types = set()
deadline = time.monotonic() + 15
while time.monotonic() < deadline and not {"tasks.insert", "comments.insert"} <= types:
    frame = websocket.receive()
    if frame["kind"] == "event":
        types.add(frame["type"])
        highest = max(highest, frame["sequence_id"])
assert {"tasks.insert", "comments.insert"} <= types, types
websocket.send({"v": 1, "kind": "ack", "sequence_id": highest})
websocket.send({"v": 1, "kind": "resume", "after_sequence_id": 0})
replayed = websocket.receive()
assert replayed["kind"] == "event", replayed

http(
    "DELETE",
    f"/api/v1/projects/{project['id']}/members/{member['user']['id']}",
    token=owner["access_token"],
    expected=(204,),
)
deadline = time.monotonic() + 10
while time.monotonic() < deadline:
    frame = websocket.receive()
    if frame["kind"] == "authorization_changed":
        break
else:
    raise AssertionError("membership removal was not propagated")

print("WebSocket runtime scenario passed")
