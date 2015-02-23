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
ifr.remove();
// Verify that detached documents do not crash.
shouldBeUndefined("iframe.internals.setGeolocationClientMock(iframe.document)");
shouldBeUndefined("iframe.internals.setGeolocationPosition(iframe.document, 1, 2, 3)");
shouldBeUndefined("iframe.internals.setGeolocationPermission(iframe.document, true)");
shouldBeUndefined("iframe.internals.setGeolocationPositionUnavailableError(iframe.document, 'not available')");
shouldBe("iframe.internals.numberOfPendingGeolocationPermissionRequests(iframe.document)", "-1");
