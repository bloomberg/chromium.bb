description("Tests that when Geolocation permission has been denied prior to a call to a Geolocation method, the error callback is invoked with code PERMISSION_DENIED, when the Geolocation service has a good position.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var error;

geolocationServiceMock.then(mock => {

    // Prime the Geolocation instance by denying permission.
    mock.setGeolocationPermission(false);
    mock.setGeolocationPosition(51.478, -0.166, 100);

    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        debug('');
        continueTest();
    });

    function continueTest()
    {
        // Make another request, with permission already denied.
        navigator.geolocation.getCurrentPosition(function(p) {
            testFailed('Success callback invoked unexpectedly');
            finishJSTest();
        }, function(e) {
            error = e;
            shouldBe('error.code', 'error.PERMISSION_DENIED');
            shouldBe('error.message', '"User denied Geolocation"');
            finishJSTest();
        });
    }
});

window.jsTestIsAsync = true;
