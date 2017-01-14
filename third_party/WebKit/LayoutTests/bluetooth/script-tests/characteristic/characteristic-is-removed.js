'use strict';
promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('generic_access'))
    .then(service => service.getCharacteristic('gap.device_name'))
    .then(characteristic => {
      return setBluetoothFakeAdapter('MissingCharacteristicHeartRateAdapter')
        .then(() => assert_promise_rejects_with_message(
          characteristic.CALLS([
              getDescriptor(user_description.name)|
              getDescriptors(user_description.name)[UUID]|
              getDescriptors()|
              readValue()|
              writeValue(val)|
              startNotifications()]),
          new DOMException(
            'GATT Characteristic no longer exists.',
            'InvalidStateError'),
          'Characteristic got removed.'));
    });
}, 'Characteristic gets removed. Reject with InvalidStateError.');

