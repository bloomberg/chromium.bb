promise_test(() => {
  let val = new Uint8Array([1]);
  return setBluetoothFakeAdapter('DisconnectingDuringSuccessGATTOperationAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['health_thermometer']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('health_thermometer'))
    .then(service => service.getCharacteristic('measurement_interval'))
    .then(characteristic => {
      let disconnected = eventPromise(characteristic.service.device, 'gattserverdisconnected');
      let promise = assert_promise_rejects_with_message(
        characteristic.CALLS([
          readValue()|
          writeValue(val)|
          startNotifications()]),
        new DOMException('GATT Server disconnected while performing a GATT operation.',
                         'NetworkError'));
      return disconnected.then(() => characteristic.service.device.gatt.connect())
                         .then(() => promise);
    });
}, 'Device reconnects during a FUNCTION_NAME call that succeeds. Reject ' +
   'with NetworkError.');
