'use strict';
promise_test(() => {
  return getHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}],
      optionalServices: ['glucose']})
    .then(([device]) => assert_promise_rejects_with_message(
      device.gatt.CALLS([
        getPrimaryService('glucose')|
        getPrimaryServices('glucose')[UUID]
      ]),
      new DOMException(
        'No Services matching UUID ' + glucose.uuid + ' found in Device.',
        'NotFoundError')));
}, 'Request for absent service. Reject with NotFoundError.');
