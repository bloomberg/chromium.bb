'use strict';
promise_test(
    () => {
      let promise;
      return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
          .then(
              () => requestDeviceWithKeyDown(
                  {filters: [{services: ['health_thermometer']}]}))
          .then(device => device.gatt.connect())
          .then(
              gattServer => gattServer.getPrimaryService('health_thermometer'))
          .then(service => service.getCharacteristic('measurement_interval'))
          .then(
              characteristic =>
                  characteristic.getDescriptor(user_description.name))
          .then(descriptor => {
            promise = assert_promise_rejects_with_message(
                descriptor.CALLS([readValue()]),
                new DOMException(
                    'GATT Server disconnected while performing a GATT operation.',
                    'NetworkError'));
            // Disconnect called to clear attributeInstanceMap and allow the
            // object to get garbage collected.
            descriptor.characteristic.service.device.gatt.disconnect();
          })
          .then(runGarbageCollection)
          .then(() => promise);
    },
    'Garbage collection ran during a FUNCTION_NAME call that succeeds. ' +
        'Should not crash.');
