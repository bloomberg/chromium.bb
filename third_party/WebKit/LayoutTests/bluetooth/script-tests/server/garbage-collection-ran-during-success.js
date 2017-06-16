'use strict';
promise_test(() => {
  let promise;
  return getHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}]})
    .then(({device}) => {
      promise = assert_promise_rejects_with_message(
        device.gatt.CALLS([
          getPrimaryService('health_thermometer') |
          getPrimaryServices() |
          getPrimaryServices('health_thermometer')[UUID]]),
        new DOMException(
          'GATT Server is disconnected. Cannot retrieve services. ' +
          '(Re)connect first with `device.gatt.connect`.',
          'NetworkError'));
      device.gatt.disconnect();
    })
    .then(runGarbageCollection)
    .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that succeeds. ' +
   'Should not crash.');
