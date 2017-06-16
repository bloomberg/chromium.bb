'use strict';
promise_test(() => {
  return getHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}],
      optionalServices: ['generic_access']})
    .then(({device}) => {
      device.gatt.disconnect();
      return assert_promise_rejects_with_message(
        device.gatt.CALLS([
          getPrimaryService('health_thermometer')|
          getPrimaryServices()|
          getPrimaryServices('health_thermometer')[UUID]]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
    });
}, 'disconnect() called before FUNCTION_NAME. Reject with NetworkError.');
