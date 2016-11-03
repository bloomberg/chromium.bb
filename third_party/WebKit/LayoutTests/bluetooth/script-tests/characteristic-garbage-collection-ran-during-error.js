'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('MissingCharacteristicHeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => gattServer.getPrimaryService('heart_rate'))
    .then(service => {
      promise = assert_promise_rejects_with_message(
        service.CALLS([
          getCharacteristic('measurement_interval')|
          getCharacteristics()|
          getCharacteristics('measurement_interval')[UUID]]),
        new DOMException(
          'GATT Server disconnected while retrieving characteristics.',
          'NetworkError'));
      // Disconnect called to clear attributeInstanceMap and allow the
      // object to get garbage collected.
      service.device.gatt.disconnect();
    })
    .then(runGarbageCollection)
    .then(() => promise);
}, 'Garbage Collection ran during FUNCTION_NAME call that fails. ' +
   'Should not crash');
