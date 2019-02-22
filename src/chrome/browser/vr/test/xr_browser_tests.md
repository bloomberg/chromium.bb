# XR Browser Tests

## Introduction

This documentation concerns `xr_browser_test.h`, `xr_browser_test.cc`, and files
that use them or their subclasses.

These files port the framework used by XR instrumentation tests (located in
[`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/`][1] and
documented in
`//chrome/android/javatests/src/org/chromium/chrome/browser/vr/*.md`) for
use in browser tests in order to test XR features on desktop platforms.

This is pretty much a direct port, with the same JavaScript/HTML files being
used for both and the Java/C++ code being functionally equivalent to each other,
so the instrumentation test's documentation on writing tests using the framework
is applicable here, too. As such, this documentation only covers any notable
differences between the two implementations.

## Restrictions

Both the instrumentation tests and browser tests have hardware/software
restrictions - in the case of browser tests, XR is only supported on Windows 8
and later (or Windows 7 with a non-standard patch applied) with a GPU that
supports DirectX 11.1.

Instrumentation tests handle restrictions with the `@Restriction` annotation,
but browser tests don't have any equivalent functionality. Instead, test names
should be wrapped in the `REQUIRES_GPU` macro defined in `xr_browser_test.h`,
which simply disables the test by default. We then explicitly run tests that
inherit from `XrBrowserTestBase` and enable the running of disabled tests on
bots that meet the requirements.

## Command Line Switches

Instrumentation tests are able to add and remove command line switches on a
per-test-case basis using `@CommandLine` annotations, but equivalent
functionality does not exist in browser tests.

Instead, if different command line flags are needed, a new class will need to
be created that extends the correct type of `*BrowserTestBase` and overrides the
flags that are set in its `SetUp` function.

## Compiling And Running

The tests are compiled as part of the standard `browser_tests` target, although
the files are currently only included on Windows.

Once compiled, the tests can be run using the following command line, which
ensures that only the XR browser tests are run:

`browser_tests.exe --enable-gpu --test-launcher-jobs=1
--gtest_filter=WebVrBrowserTest*:WebXrVrBrowserTest*
--enable-pixel-output-in-tests --gtest_also_run_disabled_tests`

Note that this must be run from your output directory, e.g. `out/Debug`, as
otherwise the setup code to use the mock OpenVR client will fail.

[1]: https://chromium.googlesource.com/chromium/src/+/master/chrome/android/javatests/src/org/chromium/chrome/browser/vr