'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('HeartRateAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['generic_access']}))
    .then(device => device.gatt.connect())
    .then(gatt => gatt.getPrimaryService('generic_access'))
    .then(service => {
      return assert_promise_rejects_with_message(
        service.CALLS([
          getCharacteristic('wrong_name')|
          getCharacteristics('wrong_name')
        ]),
        new DOMException(
          'Failed to execute \'FUNCTION_NAME\' on ' +
          '\'BluetoothRemoteGATTService\': Invalid Characteristic name: ' +
          '\'wrong_name\'. ' +
          'It must be a valid UUID alias (e.g. 0x1234), ' +
          'UUID (lowercase hex characters e.g. ' +
          '\'00001234-0000-1000-8000-00805f9b34fb\'), ' +
          'or recognized standard name from ' +
          'https://www.bluetooth.com/specifications/gatt/characteristics' +
          ' e.g. \'aerobic_heart_rate_lower_limit\'.',
          'TypeError'),
        'Wrong Characteristic name passed.');
    });
}, 'Wrong Characteristic name. Reject with TypeError.');
