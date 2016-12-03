'use strict';
promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => {
      return gattServer
        .getPrimaryService('health_thermometer')
        .then(service => service.getCharacteristic('measurement_interval'))
        .then(measurement_interval => {
          let promise = assert_promise_rejects_with_message(
            measurement_interval.CALLS([
              readValue()|
              writeValue(val)|
              startNotifications()|
              stopNotifications()]),
            new DOMException(
              'GATT Server disconnected while performing a GATT operation.',
              'NetworkError'));
          gattServer.disconnect();
          return promise;
        });
    });
}, 'disconnect() called during a FUNCTION_NAME call that succeeds. ' +
   'Reject with NetworkError.');
