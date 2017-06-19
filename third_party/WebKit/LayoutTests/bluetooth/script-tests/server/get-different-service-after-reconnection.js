'use strict';
promise_test(() => {
  return getHealthThermometerDevice({
      filters: [{services: ['health_thermometer']}],
      optionalServices: ['generic_access']})
    .then(([device]) => {
      let services_first_connection;
      return device.gatt.CALLS([
          getPrimaryService('health_thermometer')|
          getPrimaryServices()|
          getPrimaryServices('health_thermometer')[UUID]])
        .then(services => services_first_connection = services)
        .then(() => device.gatt.disconnect())
        .then(() => device.gatt.connect())
        .then(() => device.gatt.PREVIOUS_CALL)
        .then(services_second_connection => [
          services_first_connection,
          services_second_connection
        ]);
    })
    .then(([services_first_connection, services_second_connection]) => {
      // Convert to arrays if necessary.
      services_first_connection = [].concat(services_first_connection);
      services_second_connection = [].concat(services_second_connection);

      assert_equals(services_first_connection.length, services_second_connection.length);

      let first_connection_set = new Set(services_first_connection);
      let second_connection_set = new Set(services_second_connection);

      // The two sets should be disjoint.
      let common_services = services_first_connection.filter(
        val => second_connection_set.has(val));
      assert_equals(common_services.length, 0);

    });
}, 'Calls to FUNCTION_NAME after a disconnection should return a ' +
   'different object.');
