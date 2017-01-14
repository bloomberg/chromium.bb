// TODO(672127) Use this test case to test the rest of characteristic functions.
'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      return setBluetoothFakeAdapter('MissingServiceHeartRateAdapter')
        .then(() => assert_promise_rejects_with_message(
          characteristic.CALLS([
            getDescriptor(user_description.name)|
            getDescriptors(user_description.name)[UUID]|
            getDescriptors(user_description.name)]),
          new DOMException('GATT Service no longer exists.',
                           'InvalidStateError')));
      })
}, 'Service is removed. Reject with InvalidStateError.');
