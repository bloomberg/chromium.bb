'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
      .then(
          () => requestDeviceWithKeyDown(
              {filters: [{services: ['health_thermometer']}]}))
      .then(device => device.gatt.connect())
      .then(gattServer => {
        promise = assert_promise_rejects_with_message(
            gattServer.CALLS(
                [getPrimaryService('health_thermometer') |
                 getPrimaryServices() |
                 getPrimaryServices('health_thermometer')[UUID]]),
            new DOMException(
                'GATT Server is disconnected. ' +
                    'Cannot retrieve services. ' +
                    '(Re)connect first with `device.gatt.connect`.',
                'NetworkError'));
        gattServer.disconnect();
      })
      .then(runGarbageCollection)
      .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that succeeds. ' +
   'Should not crash.');
