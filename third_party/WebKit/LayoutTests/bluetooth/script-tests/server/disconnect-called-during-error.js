'use strict';
promise_test(() => {
  return getEmptyHealthThermometerDevice()
    .then(([device]) => {
      let promise = assert_promise_rejects_with_message(
        device.gatt.CALLS([
          getPrimaryService('health_thermometer')|
          getPrimaryServices()|
          getPrimaryServices('health_thermometer')[UUID]
        ]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
      device.gatt.disconnect();
      return promise;
    });
}, 'disconnect() called during a FUNCTION_NAME call that fails. Reject ' +
   'with NetworkError.');
