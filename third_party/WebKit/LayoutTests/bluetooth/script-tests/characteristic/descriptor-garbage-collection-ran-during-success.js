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
      promise = assert_promise_rejects_with_message(
        characteristic.CALLS([
          getDescriptor(user_description.name)|
          getDescriptors()|
          getDescriptors(user_description.name)[UUID]]),
        new DOMException(
          'GATT Server disconnected while performing a GATT operation.',
          'NetworkError'));
      // Disconnect called to clear attributeInstanceMap and allow the
      // object to get garbage collected.
      characteristic.service.device.gatt.disconnect();
    })
    .then(runGarbageCollection)
    .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that succeeds. ' +
   'Should not crash.');
