'use strict';
bluetooth_test(() => {
  let expected = new DOMException('Origin is not allowed to access the ' +
                                  'service. Tip: Add the service UUID to ' +
                                  '\'optionalServices\' in requestDevice() ' +
                                  'options. https://goo.gl/HxfxSQ',
                                  'SecurityError');
  return getHealthThermometerDeviceWithServicesDiscovered({
      filters: [{services: ['health_thermometer']}]})
    .then(({device}) => Promise.all([
      assert_promise_rejects_with_message(
        device.gatt.CALLS([
          getPrimaryService(glucose.alias)|
          getPrimaryServices(glucose.alias)[UUID]
        ]), expected),
      assert_promise_rejects_with_message(
        device.gatt.FUNCTION_NAME(glucose.name), expected),
      assert_promise_rejects_with_message(
        device.gatt.FUNCTION_NAME(glucose.uuid), expected)]));
}, 'Request for absent service without permission. Should Reject with ' +
   'SecurityError even if services have been discovered already.');
