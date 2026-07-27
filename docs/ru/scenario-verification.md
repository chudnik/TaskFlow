# Соответствие сценариев OpenSpec

> Источник: [scenario-verification.md](../scenario-verification.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

Матрица сопоставляет сценарии OpenSpec с исполнимыми доказательствами. Имена GoogleTest,
CTest и CI сохраняются дословно.

| Область | Сценарий | Проверка |
|---|---|---|
| Service operations | Отсутствует signing secret | `taskflow-api-missing-signing-secret`, `taskflow-worker-missing-signing-secret` |
| Service operations | Совместимая schema | `SchemaCompatibilityTest.AcceptsExactVersion`, Compose migrations |
| Service operations | Redis недоступен | `AuthenticationRateLimiter.FailsClosedWhenRedisIsUnavailable`, `NotificationWakeupTest.FallsBackToPostgresAndRecovers` |
| Service operations | API request завершён | `ApiRouterTest.PreservesSafeRequestIdAndGeneratesUnsafeOne`, `StructuredLoggerTest.FormatsRequestCompletionAsJson` |
| Service operations | Database недоступна | `ApiRouterTest.SerializesHealthContracts`; вручную: остановить PostgreSQL, `/health/ready` → 503 |
| Service operations | Pull request verification | `.github/workflows/ci.yml`, `.github/workflows/sanitizers-supply-chain.yml` |
| Deadlines/jobs | Расчёт overdue | `TaskModelTest.CalculatesOverdueAndSoftDeletes` |
| Deadlines/jobs | Приближается deadline | `ReminderSchedulerTest.ReschedulesCurrentAssignedDeadlineAndCancelsObsoleteJobs`, `ReminderHandlerTest.RevalidatesAndEmitsNoDuplicateEffect` |
| Deadlines/jobs | Deadline перенесён | `ReminderSchedulerTest.ReschedulesCurrentAssignedDeadlineAndCancelsObsoleteJobs` |
| Deadlines/jobs | Retry transient failure | `JobWorkerTest.DispatchesRetriesLogsAndStopsGracefully`, `ProjectRepositoriesIntegration.LeasesRetriesRecoversAndTerminatesJobsAcrossWorkers` |
| Deadlines/jobs | Исчерпан retry limit | `ProjectRepositoriesIntegration.LeasesRetriesRecoversAndTerminatesJobsAcrossWorkers` |
| Identity/access | Успешная регистрация | `IdentityUseCases.ControllerMapsDuplicateEmailToConflict` |
| Identity/access | Duplicate email | `IdentityUseCases.ControllerMapsDuplicateEmailToConflict` |
| Identity/access | Успешный login | `IdentityUseCases.LoginDoesNotDistinguishMissingUserAndWrongPassword`, `IdentitySecurity.Argon2idRoundTripUsesUniqueSaltAndRejectsWrongCredential` |
| Identity/access | Refresh rotation/replay | `RefreshTokenIntegration.RotatesOnceAndReplayRevokesTokenFamily` |
| Identity/access | Expired access token | `JwtAccessToken.RejectsWrongIssuerAudienceSignatureAndExpiry` |
| Identity/access | Non-member читает проект | `MembershipUseCasesTest.HidesMembershipListFromNonMembersAndRejectsDuplicates` |
| Identity/access | Admin moderation | `AuthorizationPolicy.EnforcesDenyByDefaultRoleMatrix`, `TaskUseCasesTest.EnforcesCommentAuthorAndModeratorContract` |
| Projects | Создание проекта | `ProjectUseCasesTest.CreatesProjectAndListsOnlyVisibleProjects`, `ProjectRepositoriesIntegration.CreatesProjectAndOwnerAtomically` |
| Projects | Mutation архивного проекта | `MembershipUseCasesTest.RejectsMembershipMutationsAfterArchive`, `ProjectUseCasesTest.EnforcesReadUpdateAndArchivePolicies` |
| Projects | Добавление member | `MembershipUseCasesTest.OwnerAddsChangesListsAndRemovesMemberships`, `ProjectRepositoriesIntegration.PersistsMembershipLifecycleForAllProjectRoles` |
| Projects | Удаление последнего owner | `ProjectRepositoriesIntegration.RefusesToRemoveOrDemoteFinalOwner` |
| Projects | Видимый список | `ProjectUseCasesTest.CreatesProjectAndListsOnlyVisibleProjects`, `ProjectRepositoriesIntegration.UpdatesArchivesAndListsVisibleProjects` |
| Tasks | Создание task | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse` |
| Tasks | Stale update | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse`, `ProjectRepositoriesIntegration.OptimisticallyUpdatesAndUnassignsTasksOnMemberRemoval` |
| Tasks | Завершение/invalid transition | `TaskModelTest.EnforcesWorkflowAndCompletionTimestamp`, `TaskUseCasesTest.RejectsUnauthorizedAndInvalidAssignmentsAndTransitions` |
| Tasks | Назначение member/non-member | `TaskModelTest.RequiresActiveMemberForAssignment`, `TaskUseCasesTest.RejectsUnauthorizedAndInvalidAssignmentsAndTransitions` |
| Tasks | Manager удаляет task | `TaskUseCasesTest.ImplementsAuthorizedLifecycleAndStaleResponse` |
| Task query | Комбинация filters | `TaskQueryTest.NormalizesAndParameterizesEveryFilter`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Равные sort values | `TaskQueryTest.ProducesStableFingerprintAndSortParsing`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Next page | `TaskCursorTest.RoundTripsNullAndNonNullSortValues`, `ProjectRepositoriesIntegration.FiltersSortsAndKeysetPaginatesAcrossConcurrentInsert` |
| Task query | Cursor/query mismatch | `TaskCursorTest.RejectsTamperingAndQueryMismatch` |
| Realtime | Valid/expired connection | `WebSocketGatewayTest.AuthenticatesBoundsExpiresAndCleansConnections` |
| Realtime | Task event и removed member | `ProjectRepositoriesIntegration.MaterializesOrdersReplaysAndReauthorizesNotifications` |
| Realtime | Replay retained events | `WebSocketProtocolTest.ParsesMonotonicControlFrames`, `ProjectRepositoriesIntegration.MaterializesOrdersReplaysAndReauthorizesNotifications` |
| Realtime | Replay window/slow consumer | `WebSocketProtocolTest.ParsesMonotonicControlFrames`, `WebSocketGatewayTest.AuthenticatesBoundsExpiresAndCleansConnections` |
| Collaboration | Add/edit comment | `TaskUseCasesTest.EnforcesCommentAuthorAndModeratorContract` |
| Collaboration | Task history | `ProjectRepositoriesIntegration.CommitsMatchingAuditAndOutboxForEveryMutation` |
| Collaboration | Rollback | `ProjectRepositoriesIntegration.RollsBackAndClaimsOutboxIdempotentlyWithSanitizedAudit` |
| Collaboration | Member читает history | `AuditTest.RedactsSensitiveFieldsWithoutDroppingBusinessContext`, `ProjectRepositoriesIntegration.CommitsMatchingAuditAndOutboxForEveryMutation` |

Проверка readiness при недоступной database остаётся ручным fault-injection сценарием,
поскольку требует остановки Compose dependency. Остальные сценарии имеют automated mapping.
