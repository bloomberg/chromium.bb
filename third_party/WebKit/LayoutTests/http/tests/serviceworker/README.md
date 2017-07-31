This directory contains Chromium-specific tests for service workers, i.e., tests
that are not suitable for adding to [Web Platform
Tests](/docs/testing/web_platform_tests.md). Generally, new tests should be
added to external/wpt/service-workers instead of here.

Tests in this directory should have a comment explaining why the test cannot be
upstreamed to WPT. Tests here fall into one or more of the following categories:

1) Tests that should be upstreamed to WPT, but we just have not done so.
Eventually this category should be eliminated. See https://crbug.com/688116.

2) Tests that assert behavior that contradicts the specification. Ideally we
would fix Chromium's behavior, but in the meantime sometimes need to maintain
the deprecated behavior until it can be fixed.

Instead of a Chromium-only test, it's preferable to have a WPT test that asserts
the correct things, and an -expected.txt file for Chromium's failing
expectations. However, in some cases that would not provide sufficient test
coverage for Chromium's spec-violating behavior.

When tests like this are added, they should have a comment at the top pointing
to the correct WPT test, and link to the bug tracking removal of the Chromium
test.

3) Tests that exercise implementation specific behavior like (not) crashing and
garbage collection.

4) Tests that require use of the Internals API or other Chromium-specific APIs.
