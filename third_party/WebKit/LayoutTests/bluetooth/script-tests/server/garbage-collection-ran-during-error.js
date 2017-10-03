'use strict';
bluetooth_test(() => {
  let promise;
  return getEmptyHealthThermometerDevice()
      .then(({device}) => {
        promise = assert_promise_rejects_with_message(
          device.gatt.CALLS([
            getPrimaryService('health_thermometer')|
            getPrimaryServices()|
            getPrimaryServices('health_thermometer')[UUID]
          ]),
          new DOMException(
            'GATT Server is disconnected. ' +
            'Cannot retrieve services. ' +
            '(Re)connect first with `device.gatt.connect`.',
            'NetworkError'));
        // Disconnect called to clear attributeInstanceMap and allow the
        // object to get garbage collected.
        device.gatt.disconnect();
      })
      .then(runGarbageCollection)
      .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that failed. ' +
   'Should not crash.');
