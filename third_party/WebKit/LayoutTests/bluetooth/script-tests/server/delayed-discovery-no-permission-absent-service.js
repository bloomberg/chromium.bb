'use strict';
promise_test(() => {
  let expected = new DOMException('Origin is not allowed to access the ' +
                                  'service. Tip: Add the service UUID to ' +
                                  '\'optionalServices\' in requestDevice() ' +
                                  'options. https://goo.gl/HxfxSQ',
                                  'SecurityError');
  return setBluetoothFakeAdapter('DelayedServicesDiscoveryAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{services: ['heart_rate']}]}))
    .then(device => device.gatt.connect())
    .then(gatt => Promise.all([
      assert_promise_rejects_with_message(
        gatt.CALLS([
          getPrimaryService(glucose.alias)|
          getPrimaryServices(glucose.alias)[UUID]
        ]), expected),
      assert_promise_rejects_with_message(
        gatt.FUNCTION_NAME(glucose.name), expected),
      assert_promise_rejects_with_message(
        gatt.FUNCTION_NAME(glucose.uuid), expected)]));
}, 'Delayed services discovered, request for absent service without ' +
   'permission. Reject with SecurityError.');
