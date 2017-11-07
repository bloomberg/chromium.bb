'use strict';
bluetooth_test(
    () => {
      let val = new Uint8Array([1]);
      return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
          .then(
              () => requestDeviceWithTrustedClick(
                  {filters: [{services: ['health_thermometer']}]}))
          .then(device => device.gatt.connect())
          .then(gattServer => {
            return gattServer.getPrimaryService('health_thermometer')
                .then(
                    service =>
                        service.getCharacteristic('measurement_interval'))
                .then(
                    characteristic =>
                        characteristic.getDescriptor(user_description.name))
                .then(descriptor => {
                  let promise = assert_promise_rejects_with_message(
                      descriptor.CALLS([readValue()|writeValue(val)]),
                      new DOMException(
                        'GATT Server is disconnected. Cannot perform GATT operations. ' +
                          '(Re)connect first with `device.gatt.connect`.',
                          'NetworkError'));
                  gattServer.disconnect();
                  return promise;
                });
          });
    },
    'disconnect() called during a FUNCTION_NAME call that succeeds. ' +
        'Reject with NetworkError.');
