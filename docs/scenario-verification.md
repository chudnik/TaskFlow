# OpenSpec scenario verification

This matrix maps every scenario in
`openspec/changes/build-multiuser-task-service/specs` to executable evidence. Test names
refer to GoogleTest cases unless a CTest or CI entry is named explicitly.

| Capability | OpenSpec scenario | Verification |
|---|---|---|
| Service operations | Missing signing secret | CTest `taskflow-api-missing-signing-secret`, `taskflow-worker-missing-signing-secret` |
| Service operations | Compatible schema | `SchemaCompatibilityTest.AcceptsExactVersion` and Compose migrations |
| Service operations | Redis unavailable during durable mutation | `AuthenticationRateLimiter.FailsClosedWhenRedisIsUnavailable`, `NotificationWakeupTest.FallsBackToPostgresAndRecovers` |
| Service operations | API request completes | `ApiRouterTest.PreservesSafeRequestIdAndGeneratesUnsafeOne`, `StructuredLoggerTest.FormatsRequestCompletionAsJson` |
| Service operations | Database unavailable | `ApiRouterTest.SerializesHealthContracts`; manual: stop PostgreSQL and assert `/health/ready` returns 503 |
| Service operations | Pull request verification | `.github/workflows/ci.yml` and `.github/workflows/sanitizers-supply-chain.yml` |
| Deadlines and jobs | Overdue calculation | `TaskModelTest.CalculatesOverdueAndSoftDeletes` |
| Deadlines and jobs | Deadline approaches | `ReminderSchedulerTest.ReschedulesCurrentAssignedDeadlineAndCancelsObsoleteJobs`, `ReminderHandlerTest.RevalidatesAndEmitsNoDuplicateEffect` |
| Deadlines and jobs | Deadline moved | `ReminderSchedulerTest.ReschedulesCurrentAssignedDeadlineAndCancelsObsoleteJobs` |
| Deadlines and jobs | Worker retries transient failure | `JobWorkerTest.DispatchesRetriesLogsAndStopsGracefully`, `ProjectRepositoriesIntegration.LeasesRetriesRecoversAndTerminatesJobsAcrossWorkers` |
| Deadlines and jobs | Retry limit reached | `ProjectRepositoriesIntegration.LeasesRetriesRecoversAndTerminatesJobsAcrossWorkers` |
| Identity and access | Successful registration | `IdentityUseCases.ControllerMapsDuplicateEmailToConflict` |
| Identity and access | Duplicate email | `IdentityUseCases.ControllerMapsDuplicateEmailToConflict` |
| Identity and access | Successful login | `IdentityUseCases.LoginDoesNotDistinguishMissingUserAndWrongPassword`, `IdentitySecurity.Argon2idRoundTripUsesUniqueSaltAndRejectsWrongCredential` |
| Identity and access | Refresh token rotation | `RefreshTokenIntegration.RotatesOnceAndReplayRevokesTokenFamily` |
| Identity and access | Refresh token replay | `RefreshTokenIntegration.RotatesOnceAndReplayRevokesTokenFamily` |
| Identity and access | Expired access token | `JwtAccessToken.RejectsWrongIssuerAudienceSignatureAndExpiry` |
| Identity and access | Non-member reads a project | `MembershipUseCasesTest.HidesMembershipListFromNonMembersAndRejectsDuplicates` |
| Identity and access | Admin moderation access | `AuthorizationPolicy.EnforcesDenyByDefaultRoleMatrix`, `TaskUseCasesTest.EnforcesCommentAuthorAndModeratorContract` |
| Project management | Create project | `ProjectUseCasesTest.CreatesProjectAndListsOnlyVisibleProjects`, `ProjectRepositoriesIntegration.CreatesProjectAndOwnerAtomically` |
| Project management | Archived project mutation | `MembershipUseCasesTest.RejectsMembershipMutationsAfterArchive`, `ProjectUseCasesTest.EnforcesReadUpdateAndArchivePolicies` |
| Project management | Add member | `MembershipUseCasesTest.OwnerAddsChangesListsAndRemovesMemberships`, `ProjectRepositoriesIntegration.PersistsMembershipLifecycleForAllProjectRoles` |
| Project management | Remove final owner | `ProjectRepositoriesIntegration.RefusesToRemoveOrDemoteFinalOwner` |
| Project management | User lists projects | `ProjectUseCasesTest.CreatesProjectAndListsOnlyVisibleProjects`, `ProjectRepositoriesIntegration.UpdatesArchivesAndListsVisibleProjects` |
| Task management | Create task | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse` |
| Task management | Concurrent stale update | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse`, `ProjectRepositoriesIntegration.OptimisticallyUpdatesAndUnassignsTasksOnMemberRemoval` |
| Task management | Complete active task | `TaskModelTest.EnforcesWorkflowAndCompletionTimestamp` |
| Task management | Invalid transition | `TaskUseCasesTest.RejectsUnauthorizedAndInvalidAssignmentsAndTransitions` |
| Task management | Assign project member | `TaskModelTest.RequiresActiveMemberForAssignment`, `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse` |
| Task management | Assign non-member | `TaskUseCasesTest.RejectsUnauthorizedAndInvalidAssignmentsAndTransitions` |
| Task management | Manager deletes task | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse` |
| Task query | Combine filters | `TaskQueryTest.NormalizesAndParameterizesEveryFilter`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Equal primary sort values | `TaskQueryTest.ProducesStableFingerprintAndSortParsing`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Fetch next page | `TaskCursorTest.RoundTripsNullAndNonNullSortValues`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Cursor does not match query | `TaskCursorTest.RejectsTamperingAndQueryMismatch` |
| Realtime notifications | Valid connection | `WebSocketGatewayTest.AuthenticatesBoundsExpiresAndCleansConnections` |
| Realtime notifications | Expired token at handshake | `WebSocketGatewayTest.AuthenticatesBoundsExpiresAndCleansConnections` |
| Realtime notifications | Task changed | `ProjectRepositoriesIntegration.MaterializesOrdersReplaysAndReauthorizesNotifications` |
| Realtime notifications | Removed member | `ProjectRepositoriesIntegration.MaterializesOrdersReplaysAndReauthorizesNotifications` |
| Realtime notifications | Replay retained events | `WebSocketProtocolTest.ParsesMonotonicControlFrames`, `ProjectRepositoriesIntegration.MaterializesOrdersReplaysAndReauthorizesNotifications` |
| Realtime notifications | Replay window expired | `WebSocketProtocolTest.ParsesMonotonicControlFrames` |
| Realtime notifications | Slow consumer | `WebSocketGatewayTest.AuthenticatesBoundsExpiresAndCleansConnections` |
| Task collaboration | Add comment | `TaskUseCasesTest.EnforcesCommentAuthorAndModeratorContract` |
| Task collaboration | Edit another member's comment | `TaskUseCasesTest.EnforcesCommentAuthorAndModeratorContract` |
| Task collaboration | Task update history | `ProjectRepositoriesIntegration.CommitsMatchingAuditAndOutboxForEveryMutation` |
| Task collaboration | Mutation rollback | `ProjectRepositoriesIntegration.RollsBackAndClaimsOutboxIdempotentlyWithSanitizedAudit` |
| Task collaboration | Member reads task history | `AuditTest.RedactsSensitiveFieldsWithoutDroppingBusinessContext`, `ProjectRepositoriesIntegration.CommitsMatchingAuditAndOutboxForEveryMutation` |

The database-unavailable readiness check remains an explicit manual fault-injection
scenario because it requires stopping a Compose dependency. All other scenarios have
an automated test mapping.
