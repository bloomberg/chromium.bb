importScripts('../../serviceworker/resources/worker-testharness.js');

promise_test(function(test) {
    return promise_rejects(
      test,
      'AbortError',
      navigator.geofencing.registerRegion(
        new CircularGeofencingRegion({latitude: 37.421999,
                                     longitude: -122.084015})),
      'registerRegion should fail with an AbortError');
  }, 'registerRegion should fail');

promise_test(function(test) {
    return promise_rejects(
      test,
      'AbortError',
      navigator.geofencing.unregisterRegion(""),
      'unregisterRegion should fail with an AbortError');
  }, 'unregisterRegion should fail');

promise_test(function(test) {
    return promise_rejects(
      test,
      'AbortError',
      navigator.geofencing.getRegisteredRegions(),
      'getRegisteredRegions should fail with an AbortError');
  }, 'getRegisteredRegions should fail');
