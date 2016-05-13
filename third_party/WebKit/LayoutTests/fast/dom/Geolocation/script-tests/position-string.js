description("Tests formatting of position.toString().");

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
        // shouldBe can't use local variables yet.
        position = p;
        shouldBe('position.coords.latitude', 'mockLatitude');
        shouldBe('position.coords.longitude', 'mockLongitude');
        shouldBe('position.coords.accuracy', 'mockAccuracy');
        shouldBe('position.toString()', '"[object Geoposition]"');
        shouldBe('position.coords.toString()', '"[object Coordinates]"');
        finishJSTest();
    }, function(e) {
        testFailed('Error callback invoked unexpectedly');
        finishJSTest();
    });
});

window.jsTestIsAsync = true;
