# Политика CI, безопасности и зависимостей

> Источник: [ci-security-policy.md](../ci-security-policy.md). Английская документация является канонической. При изменении поведения, команд или контрактов необходимо проверить обе языковые версии.

Обязательные blocking checks:

- configure/build с warnings-as-errors;
- unit и integration tests;
- formatting и clang-tidy;
- ASan и UBSan.

Sanitizer finding или test failure блокирует merge.

Conan dependency graph сохраняется как проверяемый license/dependency artifact.
Новая зависимость требует совместимой лицензии и явного обновления Conan lock/version.

OSV vulnerability scanning публикует SARIF и первоначально является non-blocking,
поскольку сопоставление C/C++ advisory с Conan может содержать false positives.
Подтверждённая remotely exploitable critical/high уязвимость runtime dependency
блокирует release и должна быть исправлена либо оформлена как exception с датой
истечения. Medium/low findings переходят в follow-up work.

Сбой scanner видим, но не скрывает результаты обязательных build, test, sanitizer
или static-analysis checks.
