'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('TwoHeartRateServicesAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gatt => {
      let promise = assert_promise_rejects_with_message(
        gatt.CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]
        ]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
      gatt.disconnect();
      return promise;
    });
}, 'disconnect() called during a FUNCTION_NAME call that succeeds. ' +
   'Reject with NetworkError.');
