'use strict';
bluetooth_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('generic_access'))
    .then(service => {
      return setBluetoothFakeAdapter('MissingServiceHeartRateAdapter')
        .then(() => assert_promise_rejects_with_message(
          service.CALLS([
            getCharacteristic('gap.device_name')|
            getCharacteristics()|
            getCharacteristics('gap.device_name')[UUID]
          ]),
          new DOMException('GATT Service no longer exists.',
                           'InvalidStateError'),
          'Service got removed.'));
    });
}, 'Service is removed before FUNCTION_NAME call. Reject with InvalidStateError.');
