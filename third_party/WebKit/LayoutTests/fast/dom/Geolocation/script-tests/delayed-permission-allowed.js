description("Tests that when a position is available, no callbacks are invoked until permission is allowed.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

geolocationServiceMock.then(mock => {
    mock.setGeolocationPosition(51.478, -0.166, 100);

    var permissionSet = false;

    function allowPermission() {
        permissionSet = true;
        mock.setGeolocationPermission(true);
    }

    navigator.geolocation.getCurrentPosition(function() {
        if (permissionSet) {
            testPassed('Success callback invoked');
            finishJSTest();
            return;
        }
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function() {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
    window.setTimeout(allowPermission, 100);
});

window.jsTestIsAsync = true;
