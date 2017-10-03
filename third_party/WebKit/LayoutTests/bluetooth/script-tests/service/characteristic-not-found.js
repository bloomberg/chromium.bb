'use strict';
bluetooth_test(() => {
  return getEmptyHealthThermometerService()
    .then(({service}) => assert_promise_rejects_with_message(
      service.CALLS([
        getCharacteristic('battery_level')|
        getCharacteristics('battery_level')[UUID]
      ]),
      new DOMException(
        'No Characteristics matching UUID ' + battery_level.uuid + ' found ' +
        'in Service with UUID ' + health_thermometer.uuid + '.',
        'NotFoundError')));
}, 'Request for absent characteristics with UUID. Reject with NotFoundError.');
