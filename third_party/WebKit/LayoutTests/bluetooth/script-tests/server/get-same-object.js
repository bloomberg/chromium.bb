'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('TwoHeartRateServicesAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gattServer => Promise.all([
      gattServer.CALLS([
        getPrimaryService('heart_rate')|
        getPrimaryServices()|
        getPrimaryServices('heart_rate')[UUID]]),
      gattServer.PREVIOUS_CALL]))
    .then(services_arrays => {
      // Convert to arrays if necessary.
      for (let i = 0; i < services_arrays.length; i++) {
        services_arrays[i] = [].concat(services_arrays[i]);
      }

      for (let i = 1; i < services_arrays.length; i++) {
        assert_equals(services_arrays[0].length, services_arrays[i].length);
      }

      let base_set = new Set(services_arrays[0]);
      for (let services of services_arrays) {
        services.forEach(service => assert_true(base_set.has(service)));
      }
    });
}, 'Calls to FUNCTION_NAME should return the same object.');
