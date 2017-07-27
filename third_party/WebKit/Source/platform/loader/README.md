# platform/loader/

This document describes how files under `platform/loader/` are organized.

## fetch

Contains files for low-level loading APIs.  The `PLATFORM_EXPORT` macro is
needed to use them from `core/`.  Otherwise they can be used only in
`platform/`.

## testing

Contains helper files for testing that are available in both
`blink_platform_unittests` and `webkit_unit_tests`.
These files are built as a part of the `platform:test_support` static library.
