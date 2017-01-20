description("Tests Geolocation when the geolocation service connection fails.");

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
