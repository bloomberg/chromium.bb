'use strict';
promise_test(() => {
  return setBluetoothFakeAdapter('DelayedServicesDiscoveryAdapter')
    .then(() => requestDeviceWithKeyDown({
      filters: [{name: 'Heart Rate Device'}]}))
    .then(device => device.gatt.connect())
    .then(gattServer => assert_promise_rejects_with_message(
        gattServer.CALLS([
          getPrimaryService('heart_rate')|
          getPrimaryServices()|
          getPrimaryServices('heart_rate')[UUID]]),
        new DOMException('Origin is not allowed to access any service. Tip: ' +
                         'Add the service UUID to \'optionalServices\' in ' +
                         'requestDevice() options. https://goo.gl/HxfxSQ',
                         'SecurityError')));
}, 'Delayed service discovery, request for present service without ' +
   'permission to access any service. Reject with SecurityError.');
</script>
