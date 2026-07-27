#!/bin/sh
set -eu

: "${TASKFLOW_JWT_SIGNING_SECRET:=integration-test-signing-secret-32-bytes}"
export TASKFLOW_JWT_SIGNING_SECRET

wait_status() {
  expected="$1"
  attempts=0
  while [ "$attempts" -lt 60 ]; do
    actual="$(curl --silent --output /dev/null --write-out '%{http_code}' \
      http://localhost:8080/health/ready || true)"
    if [ "$actual" = "$expected" ]; then
      return 0
    fi
    attempts=$((attempts + 1))
    sleep 1
  done
  return 1
}

docker compose up --detach api worker
wait_status 200

docker compose stop redis
wait_status 200
docker compose start redis

docker compose stop postgres
wait_status 503
docker compose start postgres
wait_status 200

api_id="$(docker compose ps --quiet api)"
worker_id="$(docker compose ps --quiet worker)"
docker kill --signal TERM "$api_id" "$worker_id" >/dev/null

attempts=0
while [ "$attempts" -lt 30 ]; do
  api_state="$(docker inspect --format '{{.State.Status}} {{.State.ExitCode}}' "$api_id")"
  worker_state="$(docker inspect --format '{{.State.Status}} {{.State.ExitCode}}' "$worker_id")"
  if [ "$api_state" = "exited 0" ] && [ "$worker_state" = "exited 0" ]; then
    docker compose up --detach api worker
    wait_status 200
    [ "$(docker compose ps --status running --services worker)" = "worker" ]
    exit 0
  fi
  attempts=$((attempts + 1))
  sleep 1
done

echo "API or worker did not terminate cleanly after SIGTERM" >&2
exit 1
