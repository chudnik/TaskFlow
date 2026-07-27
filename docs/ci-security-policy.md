# CI security and dependency policy

Mandatory blocking checks are configure/build with warnings-as-errors, unit and
integration tests, formatting, clang-tidy, ASan, and UBSan. A sanitizer finding
or test failure blocks merge.

The Conan dependency graph is retained as a reviewable license/dependency
artifact. New dependencies require a compatible license and an explicit Conan
lock/version update.

OSV vulnerability scanning uploads SARIF and is initially non-blocking because
C/C++ advisory-to-Conan matching can contain unresolved or ecosystem-mapping
false positives. A confirmed remotely exploitable critical/high advisory in a
runtime dependency blocks release and must be fixed or documented with an
expiry-bound exception. Medium/low findings are triaged into follow-up work.
Scanner execution failure is visible but does not hide the mandatory build,
test, sanitizer, or static-analysis results.
