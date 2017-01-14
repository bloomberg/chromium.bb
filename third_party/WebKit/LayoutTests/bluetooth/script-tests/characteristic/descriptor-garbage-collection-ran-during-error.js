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
        /* 0x0101 doesn't exist in this characteristic */
        characteristic.CALLS([
          getDescriptor(0x0101)|
          getDescriptors()|
          getDescriptors(0x0101)[UUID]]),
        new DOMException(
          'GATT Server disconnected while performing a GATT operation.',
          'NetworkError'));
      // Disconnect called to clear attributeInstanceMap and allow the
      // object to get garbage collected.
      characteristic.service.device.gatt.disconnect();
    })
    .then(runGarbageCollection)
    .then(() => promise);
}, 'Garbage Collection ran during FUNCTION_NAME call that fails. ' +
   'Should not crash');
