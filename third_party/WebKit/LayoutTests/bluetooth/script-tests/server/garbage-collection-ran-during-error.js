'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('MissingServiceHeartRateAdapter')
      .then(
          () =>
              requestDeviceWithKeyDown({filters: [{services: ['heart_rate']}]}))
      .then(device => device.gatt.connect())
      .then(gattServer => {
        promise = assert_promise_rejects_with_message(
            gattServer.CALLS(
                [getPrimaryService('heart_rate') | getPrimaryServices() |
                 getPrimaryServices('heart_rate')[UUID]]),
            new DOMException(
                'GATT Server is disconnected. ' +
                    'Cannot retrieve services. ' +
                    '(Re)connect first with `device.gatt.connect`.',
                'NetworkError'));
        // Disconnect called to clear attributeInstanceMap and allow the
        // object to get garbage collected.
        gattServer.disconnect();
      })
      .then(runGarbageCollection)
      .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that failed. ' +
   'Should not crash.');
