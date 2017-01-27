'use strict';
promise_test(
    () => {
        return setBluetoothFakeAdapter('DisconnectingHealthThermometerAdapter')
            .then(
                () => requestDeviceWithKeyDown(
                    {filters: [{services: ['health_thermometer']}]}))
            .then(device => device.gatt.connect())
            .then(
                gattServer =>
                    gattServer.getPrimaryService('health_thermometer'))
            .then(service => service.getCharacteristic('measurement_interval'))
            .then(
                characteristic => characteristic.getDescriptor(
                    'bad3ec61-3cc3-4954-9702-7977df514114'))
            .then(descriptor => {
              return assert_promise_rejects_with_message(
                  descriptor.CALLS([readValue()]),
                  new DOMException(
                      'readValue() called on blocklisted object marked exclude-reads. ' +
                          'https://goo.gl/4NeimX',
                      'SecurityError'));
            })},
    'Attempt to call FUNCTION_NAME on a blocked descriptor must generate a SecurityError');
