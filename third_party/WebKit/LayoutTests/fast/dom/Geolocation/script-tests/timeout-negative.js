description("Tests that when timeout is negative (and maximumAge is too), the error callback is called immediately with code TIMEOUT.");

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPosition(document, 51.478, -0.166, 100.0);

var error;
navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
}, function(e) {
    error = e;
    shouldBe('error.code', 'error.TIMEOUT');
    shouldBe('error.message', '"Timeout expired"');
    finishJSTest();
}, {
    timeout: -1000
});

window.jsTestIsAsync = true;

