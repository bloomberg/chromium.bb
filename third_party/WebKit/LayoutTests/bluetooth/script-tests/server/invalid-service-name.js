'use strict';
promise_test(() => {
  return getHealthThermometerDevice()
    .then(({device}) => {
      return assert_promise_rejects_with_message(
        device.gatt.CALLS([
          getPrimaryService('wrong_name')|
          getPrimaryServices('wrong_name')
        ]),
        new DOMException(
          'Failed to execute \'FUNCTION_NAME\' on ' +
          '\'BluetoothRemoteGATTServer\': Invalid Service name: ' +
          '\'wrong_name\'. ' +
          'It must be a valid UUID alias (e.g. 0x1234), ' +
          'UUID (lowercase hex characters e.g. ' +
          '\'00001234-0000-1000-8000-00805f9b34fb\'), ' +
          'or recognized standard name from ' +
          'https://www.bluetooth.com/specifications/gatt/services' +
          ' e.g. \'alert_notification\'.',
          'TypeError'),
        'Wrong Service name passed.');
    });
}, 'Wrong Service name. Reject with TypeError.');
