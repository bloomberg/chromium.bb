'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['glucose']}))
    .then(device => device.gatt.connect())
    .then(gatt => assert_promise_rejects_with_message(
      gatt.CALLS([
        getPrimaryService('glucose')|
        getPrimaryServices('glucose')[UUID]
      ]),
      new DOMException(
        'No Services matching UUID ' + glucose.uuid + ' found in Device.',
        'NotFoundError')));
}, 'Request for absent service. Reject with NotFoundError.');
