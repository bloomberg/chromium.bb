description("Tests Geolocation when permission is denied, using the mock service.");

var error;

geolocationServiceMock.then(mock => {

    mock.setGeolocationPermission(false);
    mock.setGeolocationPosition(51.478, -0.166, 100.0);

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
