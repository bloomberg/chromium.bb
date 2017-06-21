'use strict';
promise_test(() => {
  return getHIDDevice({filters: [{services: ['device_information']}]})
    .then(({device}) => device.gatt.getPrimaryService('device_information'))
    .then(service => {
      return assert_promise_rejects_with_message(
        service.CALLS([
          getCharacteristic('serial_number_string')|
          getCharacteristics('serial_number_string')[UUID]
        ]),
        new DOMException('getCharacteristic(s) called with blocklisted UUID. ' +
                         'https://goo.gl/4NeimX',
                         'SecurityError'),
        'Serial Number String characteristic is blocklisted.');
    });
}, 'Serial Number String characteristic is blocklisted. Should reject with ' +
   'SecurityError.');
