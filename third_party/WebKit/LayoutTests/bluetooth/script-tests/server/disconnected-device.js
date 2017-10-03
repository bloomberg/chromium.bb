'use strict';
bluetooth_test(() => {
  return getDiscoveredHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}],
      optionalServices: ['generic_access']
    })
    .then(({device}) => assert_promise_rejects_with_message(
      device.gatt.CALLS([
        getPrimaryService('health_thermometer')|
        getPrimaryServices()|
        getPrimaryServices('health_thermometer')[UUID]
      ]),
      new DOMException('GATT Server is disconnected. ' +
                       'Cannot retrieve services. ' +
                       '(Re)connect first with `device.gatt.connect`.',
                       'NetworkError')));
}, 'FUNCTION_NAME called before connecting. Reject with NetworkError.');
