# Release verification

## First releasable configuration

- Release: `0.1.0`
- Verified: 2026-07-28
- Language: C++20
- Build: CMake `ci-release` preset with warnings as errors
- Dependencies: Conan 2.30.0 lock/version policy from `conanfile.py`
- Runtime images: Ubuntu 24.04; PostgreSQL 17.10; Redis 7.4.9
- Database schema: version 3 (`V0001` through `V0003`)
- API contract: OpenAPI 3.1, `/api/v1`

## Verification evidence

The release candidate was built from repository sources inside Docker Compose and
executed against real PostgreSQL and Redis services:

```text
92/92 CTest tests passed
integration.DockerComposeEndToEndSmoke.CoversFirstReleaseLifecycle passed
integration.health-endpoints passed
OpenApiContractTest.IsValidJsonAndCoversVersionedResources passed
OpenSpec strict validation passed
clang-format check passed
AddressSanitizer audit/outbox regression passed
```

The Compose E2E scenario covers registration and login, project membership,
task and comment lifecycle, transactional outbox materialization, ordered
notification replay/acknowledgement, and deadline-job leasing/completion.

## Known limitations

- Redis Pub/Sub is an acceleration path; PostgreSQL remains the durable replay
  source and recovery can add notification latency.
- Notification delivery is at-least-once, so clients must deduplicate by event ID.
- Audit and notification tables use retention/indexing but are not partitioned.
- TLS termination and trusted proxy enforcement belong to the deployment edge.
- The local `tidy` target requires `clang-tidy` to be installed; the current
  workstation did not provide it. CI installs and runs the configured target.
- The current Drogon executable exposes version and health routing. Product
  controllers are verified through application/transport contracts and the
  Compose E2E service graph; mounting the complete public REST/WebSocket surface
  in the executable is required before an internet-facing deployment.

This configuration is suitable as the first reproducible backend-core release
and integration baseline. It is not approved for an internet-facing production
deployment until the final routing limitation is removed.
