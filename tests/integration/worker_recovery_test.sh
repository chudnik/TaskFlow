#!/bin/sh
set -eu

: "${TASKFLOW_JWT_SIGNING_SECRET:=integration-test-signing-secret-32-bytes}"
export TASKFLOW_JWT_SIGNING_SECRET

docker compose up --build --detach worker

wait_running() {
  service="$1"
  attempts=0
  while [ "$attempts" -lt 60 ]; do
    if [ "$(docker compose ps --status running --services "$service")" = "$service" ]; then
      return 0
    fi
    attempts=$((attempts + 1))
    sleep 1
  done
  return 1
}

wait_running worker
sleep 2
wait_running worker

docker compose stop postgres
sleep 2
wait_running worker
docker compose start postgres
wait_running postgres

attempts=0
while [ "$attempts" -lt 60 ]; do
  if docker compose exec -T postgres pg_isready -U taskflow -d taskflow >/dev/null 2>&1; then
    break
  fi
  attempts=$((attempts + 1))
  sleep 1
done
[ "$attempts" -lt 60 ]

docker compose exec -T postgres psql -U taskflow -d taskflow -v ON_ERROR_STOP=1 <<'SQL'
INSERT INTO jobs(id,business_key,job_type,payload,scheduled_at,max_attempts,correlation_id)
VALUES(gen_random_uuid(),'worker-recovery-probe','unknown.probe','{}',clock_timestamp(),1,
       'worker-recovery-test')
ON CONFLICT (business_key) DO UPDATE
SET status='pending',attempt=0,scheduled_at=clock_timestamp(),completed_at=NULL,
    lease_owner=NULL,lease_expires_at=NULL;
SQL

attempts=0
while [ "$attempts" -lt 60 ]; do
  state="$(docker compose exec -T postgres psql -U taskflow -d taskflow -Atc \
    "SELECT status FROM jobs WHERE business_key='worker-recovery-probe'")"
  if [ "$state" = "failed" ]; then
    exit 0
  fi
  attempts=$((attempts + 1))
  sleep 1
done

echo "worker did not resume job processing after PostgreSQL recovery" >&2
exit 1
