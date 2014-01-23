description("Tests Geolocation success callback using the mock service.");

var mockLatitude = 51.478;
var mockLongitude = -0.166;
var mockAccuracy = 100;

if (!window.testRunner || !window.internals)
    debug('This test can not run without testRunner or internals');

internals.setGeolocationClientMock(document);
internals.setGeolocationPermission(document, true);
internals.setGeolocationPosition(document,
                                 mockLatitude,
                                 mockLongitude,
                                 mockAccuracy);

var position;
navigator.geolocation.getCurrentPosition(function(p) {
    position = p;
    shouldBe('position.coords.latitude', 'mockLatitude');
    shouldBe('position.coords.longitude', 'mockLongitude');
    shouldBe('position.coords.accuracy', 'mockAccuracy');
    finishJSTest();
}, function(e) {
    testFailed('Error callback invoked unexpectedly');
    finishJSTest();
});

window.jsTestIsAsync = true;
