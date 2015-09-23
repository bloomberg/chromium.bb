description("Tests that when multiple requests are waiting for permission, no callbacks are invoked until permission is allowed.");

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPosition(document, 51.478, -0.166, 100);

var permissionSet = false;

function allowPermission() {
    permissionSet = true;
    internals.setGeolocationPermission(document, true);
}

var watchCallbackInvoked = false;
var oneShotCallbackInvoked = false;

navigator.geolocation.watchPosition(function() {
    if (permissionSet) {
        testPassed('Success callback invoked');
        watchCallbackInvoked = true;
        maybeFinishTest();
        return;
    }
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(err) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

navigator.geolocation.getCurrentPosition(function() {
    if (permissionSet) {
        testPassed('Success callback invoked');
        oneShotCallbackInvoked = true;
        maybeFinishTest();
        return;
    }
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(err) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

window.setTimeout(allowPermission, 100);

function maybeFinishTest() {
    if (watchCallbackInvoked && oneShotCallbackInvoked)
        finishJSTest();
}

window.jsTestIsAsync = true;
