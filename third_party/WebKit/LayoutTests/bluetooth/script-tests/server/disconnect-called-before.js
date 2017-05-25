'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('TwoHeartRateServicesAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      gattServer.disconnect();
      return assert_promise_rejects_with_message(
        gattServer.CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]]),
        new DOMException('GATT Server is disconnected. ' +
                         'Cannot retrieve services. ' +
                         '(Re)connect first with `device.gatt.connect`.',
                         'NetworkError'));
    });
}, 'disconnect() called before FUNCTION_NAME. Reject with NetworkError.');
