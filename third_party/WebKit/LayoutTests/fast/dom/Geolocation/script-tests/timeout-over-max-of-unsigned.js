description("Tests that when timeout value is over maximum of unsigned, the success callback is called as expected.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100.0;

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var position;

geolocationServiceMock.then(mock => {
    mock.setGeolocationPermission(true);
    mock.setGeolocationPosition(mockLatitude,
                                mockLongitude,
                                mockAccuracy);

    navigator.geolocation.getCurrentPosition(function(p) {
        position = p;
        shouldBe('position.coords.latitude', 'mockLatitude');
        shouldBe('position.coords.longitude', 'mockLongitude');
        shouldBe('position.coords.accuracy', 'mockAccuracy');
        finishJSTest();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    }, {
        timeout: 4294967296
    });
});

window.jsTestIsAsync = true;
