#!/usr/bin/env python3
"""Regression tests for the iOS C API export check (no Apple tools required)."""

from pathlib import Path
import unittest

from core_symbols import checked_symbols, public_symbols


class CoreSymbolsTest(unittest.TestCase):
    def test_current_bindings_and_weld_are_retained(self):
        mobile = Path(__file__).resolve().parents[3]
        symbols = checked_symbols(
            (mobile / "core/include/stl2stp/stl2stp.h").read_text(encoding="utf-8"),
            (mobile / "app/lib/core/ffi/bindings.dart").read_text(encoding="utf-8"),
        )
        self.assertIn("stl2stp_weld_vertices", symbols)

    def test_unknown_dart_entry_fails_before_native_build(self):
        with self.assertRaisesRegex(ValueError, "absent from the C header: stl2stp_missing"):
            checked_symbols("void stl2stp_create(void);", "'stl2stp_missing'")

    def test_new_c_entry_is_retained_without_updating_a_list(self):
        symbols = checked_symbols(
            "void stl2stp_create(void);\nvoid stl2stp_future_api(void);",
            "'stl2stp_create'",
        )
        self.assertIn("stl2stp_future_api", symbols)

    def test_comments_and_callback_typedef_are_not_exports(self):
        symbols = public_symbols(
            "/* stl2stp_example() */\n// stl2stp_other()\n"
            "typedef void (*stl2stp_callback)(int);\nvoid stl2stp_real(void);"
        )
        self.assertEqual(symbols, {"stl2stp_real"})

    def test_empty_input_is_not_silently_accepted(self):
        with self.assertRaisesRegex(ValueError, "no public"):
            checked_symbols("", "'stl2stp_create'")
        with self.assertRaisesRegex(ValueError, "no stl2stp function lookups"):
            checked_symbols("void stl2stp_create(void);", "")


if __name__ == "__main__":
    unittest.main()
