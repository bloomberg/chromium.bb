description("Verify that using invalid or detached documents on internal test methods does not crash.");

// The internals object and these test methods aren't available in production
// builds, but they are exposed to fuzzers.

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

shouldThrow("internals.setGeolocationClientMock(null);");
shouldThrow("internals.setGeolocationPosition(null, 1, 2, 3);");
shouldThrow("internals.setGeolocationPermission(window.notThere, true);");
shouldThrow("internals.setGeolocationPositionUnavailableError(null, 'not available');");
shouldThrow("internals.numberOfPendingGeolocationPermissionRequests(null)");

var ifr = document.getElementById("ifr");
var iframe = ifr.contentWindow;
// Must access it first so it exists, we cannot get internals from a detached frame window.
var iframeInternals = iframe.internals;
ifr.remove();

// Verify that detached documents do not crash.
shouldBeUndefined("iframeInternals.setGeolocationClientMock(iframe.document)");
shouldBeUndefined("iframeInternals.setGeolocationPosition(iframe.document, 1, 2, 3)");
shouldBeUndefined("iframeInternals.setGeolocationPermission(iframe.document, true)");
shouldBeUndefined("iframeInternals.setGeolocationPositionUnavailableError(iframe.document, 'not available')");
shouldBe("iframeInternals.numberOfPendingGeolocationPermissionRequests(iframe.document)", "-1");
