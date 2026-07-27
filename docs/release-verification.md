# Release verification

## First releasable configuration

- Release: `0.1.0`
- Verified: 2026-07-28
- Language: C++20
- Build: CMake `ci-release` preset with warnings as errors
- Dependencies: Conan 2.30.0 lock/version policy from `conanfile.py`
- Runtime images: Ubuntu 24.04; PostgreSQL 17.10; Redis 7.4.9
- Database schema: version 5 (`V0001` through `V0005`)
- API contract: OpenAPI 3.1, `/api/v1`

## Verification evidence

The release candidate was built from repository sources inside Docker Compose and
executed against real PostgreSQL and Redis services:

```text
93/93 dependency-backed unit/contract CTest tests passed
6/6 dependency-free developer bootstrap tests passed
18/18 PostgreSQL integration tests passed
112/112 AddressSanitizer CTest checks passed
112/112 UndefinedBehaviorSanitizer CTest checks passed
Docker Compose repository-backed HTTP scenario passed
Docker Compose WebSocket runtime scenario passed
PostgreSQL/Redis outage and process-restart recovery passed
API and worker SIGTERM graceful-shutdown checks passed
OpenApiContractTest.IsValidJsonAndCoversVersionedResources passed
OpenSpec strict validation passed
clang-format check passed
Russian documentation and checker tests passed
```

The Compose E2E scenario covers registration and login, project membership,
task and comment lifecycle, transactional outbox materialization, authorized
WebSocket delivery, ordered notification replay/acknowledgement, deadline-job
leasing/completion, membership removal, correlation, rollback, restart, and
PostgreSQL/Redis recovery.

## Known limitations

- Redis Pub/Sub is an acceleration path; PostgreSQL remains the durable replay
  source and recovery can add notification latency.
- Notification delivery is at-least-once, so clients must deduplicate by event ID.
- Audit and notification tables use retention/indexing but are not partitioned.
- TLS termination and trusted proxy enforcement belong to the deployment edge.
- The local `tidy` target requires `clang-tidy` to be installed; the current
  workstation did not provide it. CI installs and runs the configured target.
- The runtime is a backend-only release; a browser frontend is outside this
  release scope.

This configuration is suitable as a reproducible backend release and
integration baseline. Production deployment still requires environment-specific
TLS, secret management, observability, backup, and capacity validation.
