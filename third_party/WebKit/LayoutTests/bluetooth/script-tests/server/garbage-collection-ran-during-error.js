'use strict';
promise_test(() => {
  let promise;
  return setBluetoothFakeAdapter('MissingServiceHeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      promise = assert_promise_rejects_with_message(
        gattServer.CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]]),
        new DOMException(
          'GATT Server disconnected while retrieving services.',
          'NetworkError'));
      // Disconnect called to clear attributeInstanceMap and allow the
      // object to get garbage collected.
      gattServer.disconnect();
    })
    .then(runGarbageCollection)
    .then(() => promise);
}, 'Garbage Collection ran during a FUNCTION_NAME call that failed. ' +
   'Should not crash.');
