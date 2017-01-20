description("Tests Geolocation when the permission service connection fails.");

var error;

geolocationServiceMock.then(mock => {

    mock.rejectPermissionConnections();

    navigator.geolocation.getCurrentPosition(function(p) {
        testFailed('Success callback invoked unexpectedly');
        finishJSTest();
    }, function(e) {
        error = e;
        shouldBe('error.code', 'error.PERMISSION_DENIED');
        shouldBe('error.message', '"User denied Geolocation"');
        finishJSTest();
    });
});

window.jsTestIsAsync = true;
