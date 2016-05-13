description("Tests Geolocation when the geolocation service connection fails.");

if (!window.testRunner || !window.mojo)
    debug('This test can not run without testRunner or mojo');

var error;

geolocationServiceMock.then(mock => {
  mock.setGeolocationPermission(true);
  mock.rejectGeolocationConnections();

  navigator.geolocation.getCurrentPosition(function(p) {
    testFailed('Success callback invoked unexpectedly');
    finishJSTest();
  }, function(e) {
    error = e;
    shouldBe('error.code', 'error.POSITION_UNAVAILABLE');
    shouldBe('error.message', '"Failed to start Geolocation service"');
    finishJSTest();
  });
});

window.jsTestIsAsync = true;
