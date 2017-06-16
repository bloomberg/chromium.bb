'use strict';
promise_test(() => {
  let promise;
  return getHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}]})
    .then(({device}) => {
      return device.gatt.CALLS([
        getPrimaryService('health_thermometer')|
        getPrimaryServices()|
        getPrimaryServices('health_thermometer')[UUID]])
        // Convert to array if necessary.
        .then(s => {
          let services = [].concat(s);
          device.gatt.disconnect();
          return device.gatt.connect()
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
            service.getCharacteristic('measurement_interval'),
            error));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            service.getCharacteristics(),
            error));
        promises = promises.then(() =>
          assert_promise_rejects_with_message(
            service.getCharacteristics('measurement_interval'),
            error));
      }
      return promises;
    });
}, 'Calls on services after we disconnect and connect again. '+
   'Should reject with InvalidStateError.');
