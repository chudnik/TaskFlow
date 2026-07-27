#!/bin/sh
set -eu

: "${TASKFLOW_JWT_SIGNING_SECRET:=integration-test-signing-secret-32-bytes}"
export TASKFLOW_JWT_SIGNING_SECRET

docker compose up --build --detach api worker
python3 tests/integration/repository_http_test.py http://localhost:8080
python3 tests/integration/websocket_runtime_test.py http://localhost:8080
