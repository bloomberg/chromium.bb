'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('TwoHeartRateServicesAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      return gattServer
        .CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]])
        // Convert to array if necessary.
        .then(s => {
          let services = [].concat(s);
          gattServer.disconnect();
          return gattServer.connect()
            .then(() => services);
        });
    })
    .then(services => {
      let promises = Promise.resolve();
      for (let service of services) {
        let error = new DOMException(
          'Service with UUID ' + service.uuid +
          ' is no longer valid. Remember to retrieve the service ' +
          'again after reconnecting.',
          'InvalidStateError');
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            service.getCharacteristic('body_sensor_location'),
            error));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            service.getCharacteristics(),
            error));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            service.getCharacteristics('body_sensor_location'),
            error));
      }
      return promises;
    });
}, 'Calls on services after we disconnect and connect again. '+
   'Should reject with InvalidStateError.');
