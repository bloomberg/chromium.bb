importScripts('../../serviceworker/resources/worker-testharness.js');
importScripts('../../resources/testharness-helpers.js');

promise_test(function(test) {
    return assert_promise_rejects(
      navigator.geofencing.registerRegion(
        new CircularGeofencingRegion({latitude: 37.421999,
                                     longitude: -122.084015})),
      'AbortError',
      'registerRegion should fail with an AbortError');
  }, 'registerRegion should fail');

promise_test(function(test) {
    return assert_promise_rejects(
      navigator.geofencing.unregisterRegion(""),
      'AbortError',
      'unregisterRegion should fail with an AbortError');
  }, 'unregisterRegion should fail');

promise_test(function(test) {
    return assert_promise_rejects(
      navigator.geofencing.getRegisteredRegions(),
      'AbortError',
      'getRegisteredRegions should fail with an AbortError');
  }, 'getRegisteredRegions should fail');
