'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('heart_rate'))
    .then(service => assert_promise_rejects_with_message(
      service.CALLS([
        getCharacteristic('battery_level')|
        getCharacteristics('battery_level')[UUID]
      ]),
      new DOMException(
        'No Characteristics matching UUID ' + battery_level.uuid + ' found ' +
        'in Service with UUID ' + heart_rate.uuid + '.',
        'NotFoundError')));
}, 'Request for absent characteristics with UUID. Reject with NotFoundError.');
