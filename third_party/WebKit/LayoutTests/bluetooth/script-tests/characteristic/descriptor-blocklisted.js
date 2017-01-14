'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      return assert_promise_rejects_with_message(
        // Using UUIDs instead of names to avoid making a name<>uuid mapping.
        characteristic.CALLS([
          getDescriptor('bad2ddcf-60db-45cd-bef9-fd72b153cf7c')|
          getDescriptors('bad2ddcf-60db-45cd-bef9-fd72b153cf7c')]),
          new DOMException('getDescriptor(s) called with blocklisted UUID. ' +
                         'https://goo.gl/4NeimX',
              'SecurityError'));
    })
}, 'Making sure FUNCTION_NAME can not access blocklisted descriptors.');
