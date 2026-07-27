"""Tests for the dependency-free Russian documentation checker."""

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
CHECKER_PATH = REPOSITORY_ROOT / "scripts/check_russian_docs.py"
SPEC = importlib.util.spec_from_file_location("check_russian_docs", CHECKER_PATH)
assert SPEC is not None and SPEC.loader is not None
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


class RussianDocumentationCheckerTest(unittest.TestCase):
    def test_repository_documentation_passes(self) -> None:
        self.assertEqual([], CHECKER.check_repository(REPOSITORY_ROOT))

    def test_broken_local_link_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary_directory:
            document = Path(temporary_directory) / "document.md"
            document.write_text("[сломанная ссылка](missing.md)\n", encoding="utf-8")

            errors = CHECKER.check_markdown_links(document)

        self.assertEqual(1, len(errors))
        self.assertIn("missing.md", errors[0])


if __name__ == "__main__":
    unittest.main()
