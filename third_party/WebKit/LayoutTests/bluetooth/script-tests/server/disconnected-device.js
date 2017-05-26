'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']
    }))
    .then(device => assert_promise_rejects_with_message(
      device.gatt.CALLS([
        getPrimaryService('heart_rate')|
        getPrimaryServices()|
        getPrimaryServices('heart_rate')[UUID]
      ]),
      new DOMException('GATT Server is disconnected. ' +
                       'Cannot retrieve services. ' +
                       '(Re)connect first with `device.gatt.connect`.',
                       'NetworkError')));
}, 'FUNCTION_NAME called before connecting. Reject with NetworkError.');
