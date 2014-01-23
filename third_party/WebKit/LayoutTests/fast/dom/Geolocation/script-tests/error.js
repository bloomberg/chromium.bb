description("Tests Geolocation error callback using the mock service.");

var mockMessage = "debug";

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPermission(document, true);
internals.setGeolocationPositionUnavailableError(document, mockMessage);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    error = e;
    shouldBe('error.code', 'error.POSITION_UNAVAILABLE');
    shouldBe('error.message', 'mockMessage');
    shouldBe('error.UNKNOWN_ERROR', 'undefined');
    shouldBe('error.PERMISSION_DENIED', '1');
    shouldBe('error.POSITION_UNAVAILABLE', '2');
    shouldBe('error.TIMEOUT', '3');
    finishJSTest();
});

window.jsTestIsAsync = true;
