description("Tests that reentrant calls to Geolocation methods from the error callback due to a PERMISSION_DENIED error are OK.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var error;

geolocationServiceMock.then(mock => {
    mock.setGeolocationPermission(false);
    mock.setGeolocationPosition(51.478, -0.166, 100.0);

    function checkPermissionError(e) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
    }

    var errorCallbackInvoked = false;
    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        if (errorCallbackInvoked) {
            testFailed('Error callback invoked unexpectedly');
            finishJSTest();
        }
        errorCallbackInvoked = true;
        checkPermissionError(e);
        continueTest();
    });

    function continueTest() {
        navigator.geolocation.getCurrentPosition(function(p) {
            testFailed('Success callback invoked unexpectedly');
            finishJSTest();
        }, function(e) {
            checkPermissionError(e);
            finishJSTest();
        });
    }
});

window.jsTestIsAsync = true;
