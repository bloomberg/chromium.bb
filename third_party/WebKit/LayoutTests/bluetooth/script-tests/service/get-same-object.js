'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('heart_rate'))
    .then(service => Promise.all([
      service.CALLS([
        getCharacteristic('body_sensor_location')|
        getCharacteristics()|
        getCharacteristics('body_sensor_location')[UUID]]),
      service.PREVIOUS_CALL]))
    .then(characteristics_arrays => {
      // Convert to arrays if necessary.
      for (let i = 0; i < characteristics_arrays.length; i++) {
        characteristics_arrays[i] = [].concat(characteristics_arrays[i]);
      }

      for (let i = 1; i < characteristics_arrays.length; i++) {
        assert_equals(characteristics_arrays[0].length,
                      characteristics_arrays[i].length);
      }

      let base_set = new Set(characteristics_arrays[0]);
      for (let characteristics of characteristics_arrays) {
        characteristics.forEach(
          characteristic => assert_true(base_set.has(characteristic)));
      }
    });
}, 'Calls to FUNCTION_NAME should return the same object.');
