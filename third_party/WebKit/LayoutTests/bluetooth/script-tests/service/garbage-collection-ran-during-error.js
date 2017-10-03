'use strict';
let test_desc = 'Garbage Collection ran during FUNCTION_NAME call that ' +
   'fails. Should not crash';
let func_promise;
bluetooth_test(() => getHealthThermometerService()
  .then(({service}) => {
    func_promise = assert_promise_rejects_with_message(
        service.CALLS([
            getCharacteristic('measurement_interval')|
            getCharacteristics()|
            getCharacteristics('measurement_interval')[UUID]]),
        new DOMException(
            'GATT Server is disconnected. Cannot retrieve characteristics. ' +
            '(Re)connect first with `device.gatt.connect`.',
            'NetworkError'));
    // Disconnect called to clear attributeInstanceMap and allow the object to
    // get garbage collected.
    service.device.gatt.disconnect();
  })
  .then(runGarbageCollection)
  .then(() => func_promise), test_desc);
