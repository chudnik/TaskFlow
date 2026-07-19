#!/usr/bin/env python3

import json
import sys
import time
import urllib.error
import urllib.request


def request(base_url: str, path: str):
    request = urllib.request.Request(base_url + path, headers={"X-Request-ID": "health-test"})
    try:
        with urllib.request.urlopen(request, timeout=2) as response:
            return response.status, response.headers, json.load(response)
    except urllib.error.HTTPError as error:
        return error.code, error.headers, json.load(error)


def wait_for_server(base_url: str):
    for _ in range(50):
        try:
            return request(base_url, "/health/live")
        except (OSError, urllib.error.URLError):
            time.sleep(0.1)
    raise RuntimeError("API did not start")


base_url, expected_readiness = sys.argv[1:3]
live_status, live_headers, live_body = wait_for_server(base_url)
assert live_status == 200, live_status
assert live_body == {"status": "alive"}, live_body
assert live_headers["X-Request-ID"] == "health-test", live_headers

ready_status, _, ready_body = request(base_url, "/health/ready")
if expected_readiness == "ready":
    assert ready_status == 200, (ready_status, ready_body)
    assert ready_body["status"] == "ready", ready_body
    assert ready_body["checks"] == {"postgres": "available", "schema": "compatible"}, ready_body
else:
    assert ready_status == 503, (ready_status, ready_body)
    assert ready_body["status"] == "unavailable", ready_body
    assert ready_body["checks"]["postgres"] == "unavailable", ready_body
