#!/bin/sh
set -eu

api_binary=$1
script_dir=$2

if [ -z "${TASKFLOW_POSTGRES_DSN:-}" ]; then
  exit 77
fi
if [ -z "${TASKFLOW_JWT_SIGNING_SECRET:-}" ] &&
   [ -z "${TASKFLOW_JWT_SIGNING_SECRET_FILE:-}" ]; then
  exit 77
fi

api_pid=''
cleanup() {
  if [ -n "$api_pid" ]; then
    kill -TERM "$api_pid" 2>/dev/null || true
    wait "$api_pid" 2>/dev/null || true
  fi
}
trap cleanup EXIT INT TERM

run_case() {
  port=$1
  expected=$2
  dsn=$3
  TASKFLOW_POSTGRES_DSN="$dsn" TASKFLOW_HTTP_ADDRESS=127.0.0.1 TASKFLOW_HTTP_PORT="$port" \
    "$api_binary" >/tmp/taskflow-health-test.log 2>&1 &
  api_pid=$!
  python3 "$script_dir/health_endpoints_test.py" "http://127.0.0.1:$port" "$expected"
  cleanup
  api_pid=''
}

run_case 18080 ready "$TASKFLOW_POSTGRES_DSN"
run_case 18081 unavailable 'postgresql://taskflow:unused@127.0.0.1:1/unavailable?connect_timeout=1'
