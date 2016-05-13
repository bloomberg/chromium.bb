description("Tests that when a position is available, no callbacks are invoked until permission is denied.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var error;

geolocationServiceMock.then(mock => {
    mock.setGeolocationPosition(51.478, -0.166, 100);

    var permissionSet = false;

    function denyPermission() {
        permissionSet = true;
        mock.setGeolocationPermission(false);
    }

    navigator.geolocation.getCurrentPosition(function() {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        if (permissionSet) {
            error = e;
            shouldBe('error.code', 'error.PERMISSION_DENIED');
            shouldBe('error.message', '"User denied Geolocation"');
            finishJSTest();
            return;
        }
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
    window.setTimeout(denyPermission, 100);
});

window.jsTestIsAsync = true;
