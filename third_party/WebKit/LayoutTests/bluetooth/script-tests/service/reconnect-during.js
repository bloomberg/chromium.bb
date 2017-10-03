'use strict';
bluetooth_test(() => {
  let device, service;
  return getHealthThermometerDeviceWithServicesDiscovered({
    filters: [{services: [health_thermometer.name]}],
  })
    .then(_ => ({device} = _))
    .then(() => device.gatt.getPrimaryService(health_thermometer.name))
    .then(_ => service = _)
    .then(() => {
      // 1. Make a call to service.FUNCTION_NAME, while the service is still
      // valid.
      let promise = assert_promise_rejects_with_message(service.CALLS([
        getCharacteristic(measurement_interval.name)|
        getCharacteristics()|
        getCharacteristics(measurement_interval.name)[UUID]
      ]), new DOMException(
          'GATT Server is disconnected. Cannot retrieve characteristics. ' +
          '(Re)connect first with `device.gatt.connect`.',
          'NetworkError'));

      // 2. disconnect() and connect before the initial call completes.
      // This is accomplished by making the calls without waiting for the
      // earlier promises to resolve.
      // connect() guarantees on OS-level connection, but disconnect()
      // only disconnects the current instance.
      // getHealthThermometerDeviceWithServicesDiscovered holds another
      // connection in an iframe, so disconnect() and connect() are certain to
      // reconnect.  However, disconnect() will invalidate the service object so
      // the subsequent calls made to it will fail, even after reconnecting.
      device.gatt.disconnect();
      return device.gatt.connect()
        // 3. Check that the initial call did eventually fail.  To do that, we
        // must hold a reference to the first promise.
        .then(() => promise);
    });
}, 'disconnect() and connect() called during FUNCTION_NAME. Reject with ' +
   'NetworkError.');
