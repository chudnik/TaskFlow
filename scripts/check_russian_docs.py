#!/usr/bin/env python3
"""Validate the Russian documentation set without third-party dependencies."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path


REQUIRED_DOCUMENTS = (
    "docs/ru/README.md",
    "docs/ru/setup.md",
    "docs/ru/api.md",
    "docs/ru/websocket-protocol.md",
    "docs/ru/operations.md",
    "docs/ru/performance.md",
    "docs/ru/ci-security-policy.md",
    "docs/ru/scenario-verification.md",
    "docs/ru/release-verification.md",
)

REQUIRED_TOKENS = {
    "docs/ru/setup.md": (
        "cmake --preset conan-debug",
        "docker compose --profile test run --build --rm integration-tests",
        "TASKFLOW_POSTGRES_DSN",
        "TASKFLOW_REDIS_URI",
        "TASKFLOW_JWT_SIGNING_SECRET",
    ),
    "docs/ru/api.md": (
        "/api/v1/auth/register",
        "/api/v1/tasks",
        "Authorization: Bearer",
        "version",
    ),
    "docs/ru/websocket-protocol.md": (
        "/api/v1/ws",
        "event_id",
        "sequence_id",
        "resync_required",
        "slow_consumer",
        "1013",
    ),
    "docs/ru/operations.md": (
        "/health/ready",
        "pg_dump",
        "pg_restore",
        "SKIP LOCKED",
    ),
    "docs/ru/performance.md": (
        "tests/integration/performance_smoke.sql",
        "EXPLAIN (ANALYZE, BUFFERS, FORMAT JSON)",
    ),
    "docs/ru/ci-security-policy.md": (
        "clang-tidy",
        "ASan",
        "UBSan",
        "OSV",
    ),
}

MARKDOWN_LINK = re.compile(r"(?<!!)\[[^\]]+\]\(([^)]+)\)")


def check_markdown_links(document: Path) -> list[str]:
    """Return errors for missing local link targets in one Markdown document."""
    errors: list[str] = []
    text = document.read_text(encoding="utf-8")
    for raw_target in MARKDOWN_LINK.findall(text):
        target = raw_target.strip().split(maxsplit=1)[0].strip("<>")
        if (
            not target
            or target.startswith(("#", "http://", "https://", "mailto:"))
        ):
            continue
        path_part = target.split("#", 1)[0]
        if path_part and not (document.parent / path_part).resolve().exists():
            errors.append(f"{document}: отсутствует локальная ссылка {raw_target}")
    return errors


def check_repository(root: Path) -> list[str]:
    """Return all Russian-documentation validation errors for a repository."""
    errors: list[str] = []
    for relative_name in REQUIRED_DOCUMENTS:
        document = root / relative_name
        if not document.is_file():
            errors.append(f"{relative_name}: обязательный файл отсутствует")
            continue

        text = document.read_text(encoding="utf-8")
        if not re.search(r"Источники?:", text):
            errors.append(f"{relative_name}: отсутствует отметка «Источник»")
        if "каноническ" not in text:
            errors.append(
                f"{relative_name}: отсутствует правило о канонической английской версии"
            )
        for token in REQUIRED_TOKENS.get(relative_name, ()):
            if token not in text:
                errors.append(f"{relative_name}: отсутствует технический токен {token!r}")
        errors.extend(check_markdown_links(document))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
        help="корень репозитория (по умолчанию определяется по расположению скрипта)",
    )
    args = parser.parse_args()
    errors = check_repository(args.root.resolve())
    if errors:
        print("Проверка русской документации завершилась с ошибками:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print(f"Русская документация проверена: {len(REQUIRED_DOCUMENTS)} файлов.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
