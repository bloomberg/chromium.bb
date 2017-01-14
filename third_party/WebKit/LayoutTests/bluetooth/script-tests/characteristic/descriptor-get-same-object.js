'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => Promise.all([
      characteristic.CALLS([
        getDescriptor(user_description.alias),
        getDescriptor(user_description.name),
        getDescriptor(user_description.uuid)]),
      characteristic.PREVIOUS_CALL]))
    .then(descriptors_arrays => {

      assert_true(descriptors_arrays.length > 0)

      // Convert to arrays if necessary.
      for (let i = 0; i < descriptors_arrays.length; i++) {
        descriptors_arrays[i] = [].concat(descriptors_arrays[i]);
      }

      for (let i = 1; i < descriptors_arrays.length; i++) {
        assert_equals(descriptors_arrays[0].length,
                      descriptors_arrays[i].length);
      }

      let base_set = new Set(descriptors_arrays[0]);
      for (let descriptors of descriptors_arrays) {
        descriptors.forEach(
          descriptor => assert_true(base_set.has(descriptor)));
      }
    });
}, 'Calls to FUNCTION_NAME should return the same object.');
