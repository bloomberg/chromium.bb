'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('DelayedServicesDiscoveryAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}],
      optionalServices: ['battery_service']}))
    .then(device => device.gatt.connect())
    .then(gatt => assert_promise_rejects_with_message(
      gatt.CALLS([
        getPrimaryService('battery_service')|
        getPrimaryServices('battery_service')[UUID]
      ]),
      new DOMException(
        'No Services matching UUID ' + battery_service.uuid + ' found in ' +
        'Device.', 'NotFoundError')));
}, 'Request for absent service. Must reject with NotFoundError even when the ' +
   'services are not immediately discovered');
