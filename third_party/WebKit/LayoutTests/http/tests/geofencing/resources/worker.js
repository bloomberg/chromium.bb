importScripts('../../serviceworker/resources/worker-test-harness.js');

promise_test(function(test) {
    return navigator.geofencing.registerRegion(
        new CircularGeofencingRegion({latitude: 37.421999,
                                      longitude: -122.084015}))
      .then(test.unreached_func('Promise should not have resolved'))
      .catch(function() { });
  }, 'registerRegion should fail');

promise_test(function(test) {
    return navigator.geofencing.unregisterRegion("")
      .then(test.unreached_func('Promise should not have resolved'))
      .catch(function() { });
  }, 'unregisterRegion should fail');

promise_test(function(test) {
    return navigator.geofencing.getRegisteredRegions()
      .then(test.unreached_func('Promise should not have resolved'))
      .catch(function() { });
  }, 'getRegisteredRegions should fail');
