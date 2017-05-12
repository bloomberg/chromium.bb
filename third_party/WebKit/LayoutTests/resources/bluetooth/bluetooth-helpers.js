'use strict';

// Bluetooth UUID constants:
// Services:
var blocklist_test_service_uuid = "611c954a-263b-4f4a-aab6-01ddb953f985";
var request_disconnection_service_uuid = "01d7d889-7451-419f-aeb8-d65e7b9277af";
// Characteristics:
var blocklist_exclude_reads_characteristic_uuid =
  "bad1c9a2-9a5b-4015-8b60-1579bbbf2135";
var request_disconnection_characteristic_uuid =
  "01d7d88a-7451-419f-aeb8-d65e7b9277af";
// Descriptors:
var blocklist_test_descriptor_uuid = "bad2ddcf-60db-45cd-bef9-fd72b153cf7c";

// Sometimes we need to test that using either the name, alias, or UUID
// produces the same result. The following objects help us do that.
var generic_access = {
  alias: 0x1800,
  name: 'generic_access',
  uuid: '00001800-0000-1000-8000-00805f9b34fb'
};
var device_name = {
  alias: 0x2a00,
  name: 'gap.device_name',
  uuid: '00002a00-0000-1000-8000-00805f9b34fb'
};
var reconnection_address = {
  alias: 0x2a03,
  name: 'gap.reconnection_address',
  uuid: '00002a03-0000-1000-8000-00805f9b34fb'
};
var heart_rate = {
  alias: 0x180d,
  name: 'heart_rate',
  uuid: '0000180d-0000-1000-8000-00805f9b34fb'
};
var body_sensor_location = {
  alias: 0x2a38,
  name: 'body_sensor_location',
  uuid: '00002a38-0000-1000-8000-00805f9b34fb'
};
var glucose = {
  alias: 0x1808,
  name: 'glucose',
  uuid: '00001808-0000-1000-8000-00805f9b34fb'
};
var battery_service = {
  alias: 0x180f,
  name: 'battery_service',
  uuid: '0000180f-0000-1000-8000-00805f9b34fb'
};
var battery_level = {
  alias: 0x2A19,
  name: 'battery_level',
  uuid: '00002a19-0000-1000-8000-00805f9b34fb'
};
var user_description = {
  alias: 0x2901,
  name: 'gatt.characteristic_user_description',
  uuid: '00002901-0000-1000-8000-00805f9b34fb'
};
var client_characteristic_configuration = {
  alias: 0x2902,
  name: 'gatt.client_characteristic_configuration',
  uuid: '00002902-0000-1000-8000-00805f9b34fb'
};
var measurement_interval = {
  alias: 0x2a21,
  name: 'measurement_interval',
  uuid: '00002a21-0000-1000-8000-00805f9b34fb'
};

// The following tests make sure the Web Bluetooth implementation
// responds correctly to the different types of errors the
// underlying platform might return for GATT operations.

// Each browser should map these characteristics to specific code paths
// that result in different errors thus increasing code coverage
// when testing. Therefore some of these characteristics might not be useful
// for all browsers.
//
// TODO(ortuno): According to the testing spec errorUUID(0x101) to
// errorUUID(0x1ff) should be use for the uuids of the characteristics.
var gatt_errors_tests = [{
  testName: 'GATT Error: Unknown.',
  uuid: errorUUID(0xA1),
  error: new DOMException(
      'GATT Error Unknown.',
      'NotSupportedError')
}, {
  testName: 'GATT Error: Failed.',
  uuid: errorUUID(0xA2),
  error: new DOMException(
      'GATT operation failed for unknown reason.',
      'NotSupportedError')
}, {
  testName: 'GATT Error: In Progress.',
  uuid: errorUUID(0xA3),
  error: new DOMException(
      'GATT operation already in progress.',
      'NetworkError')
}, {
  testName: 'GATT Error: Invalid Length.',
  uuid: errorUUID(0xA4),
  error: new DOMException(
      'GATT Error: invalid attribute length.',
      'InvalidModificationError')
}, {
  testName: 'GATT Error: Not Permitted.',
  uuid: errorUUID(0xA5),
  error: new DOMException(
      'GATT operation not permitted.',
      'NotSupportedError')
}, {
  testName: 'GATT Error: Not Authorized.',
  uuid: errorUUID(0xA6),
  error: new DOMException(
      'GATT operation not authorized.',
      'SecurityError')
}, {
  testName: 'GATT Error: Not Paired.',
  uuid: errorUUID(0xA7),
  // TODO(ortuno): Change to InsufficientAuthenticationError or similiar
  // once https://github.com/WebBluetoothCG/web-bluetooth/issues/137 is
  // resolved.
  error: new DOMException(
      'GATT Error: Not paired.',
      'NetworkError')
}, {
  testName: 'GATT Error: Not Supported.',
  uuid: errorUUID(0xA8),
  error: new DOMException(
      'GATT Error: Not supported.',
      'NotSupportedError')
}];

// TODO(jyasskin): Upstream this to testharness.js: https://crbug.com/509058.
function callWithKeyDown(functionCalledOnKeyPress) {
  return new Promise(resolve => {
    function onKeyPress() {
      document.removeEventListener('keypress', onKeyPress, false);
      resolve(functionCalledOnKeyPress());
    }
    document.addEventListener('keypress', onKeyPress, false);

    eventSender.keyDown(' ', []);
  });
}

// Calls requestDevice() in a context that's 'allowed to show a popup'.
function requestDeviceWithKeyDown() {
  let args = arguments;
  return callWithKeyDown(() => navigator.bluetooth.requestDevice.apply(navigator.bluetooth, args));
}

function assert_testRunner() {
  assert_true(window.testRunner instanceof Object,
    "window.testRunner is required for this test, it will not work manually.");
}

function setBluetoothManualChooser(enable) {
  assert_testRunner();
  testRunner.setBluetoothManualChooser(enable);
}

// Calls testRunner.getBluetoothManualChooserEvents() until it's returned
// |expected_count| events. Or just once if |expected_count| is undefined.
function getBluetoothManualChooserEvents(expected_count) {
  assert_testRunner();
  return new Promise((resolve, reject) => {
    let events = [];
    let accumulate_events = new_events => {
      events.push(...new_events);
      if (events.length >= expected_count) {
        resolve(events);
      } else {
        testRunner.getBluetoothManualChooserEvents(accumulate_events);
      }
    };
    testRunner.getBluetoothManualChooserEvents(accumulate_events);
  });
}

function sendBluetoothManualChooserEvent(event, argument) {
  assert_testRunner();
  testRunner.sendBluetoothManualChooserEvent(event, argument);
}

function setBluetoothFakeAdapter(adapter_name) {
  assert_testRunner();
  return new Promise(resolve => {
    testRunner.setBluetoothFakeAdapter(adapter_name, resolve);
  });
}

// errorUUID(alias) returns a UUID with the top 32 bits of
// '00000000-97e5-4cd7-b9f1-f5a427670c59' replaced with the bits of |alias|.
// For example, errorUUID(0xDEADBEEF) returns
// 'deadbeef-97e5-4cd7-b9f1-f5a427670c59'. The bottom 96 bits of error UUIDs
// were generated as a type 4 (random) UUID.
function errorUUID(uuidAlias) {
  // Make the number positive.
  uuidAlias >>>= 0;
  // Append the alias as a hex number.
  var strAlias = '0000000' + uuidAlias.toString(16);
  // Get last 8 digits of strAlias.
  strAlias = strAlias.substr(-8);
  // Append Base Error UUID
  return strAlias + '-97e5-4cd7-b9f1-f5a427670c59';
}

// Function to test that a promise rejects with the expected error type and
// message.
function assert_promise_rejects_with_message(promise, expected, description) {
  return promise.then(() => {
    assert_unreached('Promise should have rejected: ' + description);
  }, error => {
    assert_equals(error.name, expected.name, 'Unexpected Error Name:');
    if (expected.message) {
      assert_equals(error.message, expected.message, 'Unexpected Error Message:');
    }
  });
}

// Parses add-device(name)=id lines in
// testRunner.getBluetoothManualChooserEvents() output, and exposes the name->id
// mapping.
class AddDeviceEventSet {
  constructor() {
    this._idsByName = new Map();
    this._addDeviceRegex = /^add-device\(([^)]+)\)=(.+)$/;
  }
  assert_add_device_event(event, description) {
    let match = this._addDeviceRegex.exec(event);
    assert_true(!!match, event + " isn't an add-device event: " + description);
    this._idsByName.set(match[1], match[2]);
  }
  has(name) {
    return this._idsByName.has(name);
  }
  get(name) {
    return this._idsByName.get(name);
  }
}

function runGarbageCollection()
{
  // Run gc() as a promise.
  return new Promise(
      function(resolve, reject) {
        GCController.collect();
        setTimeout(resolve, 0);
      });
}

function eventPromise(target, type, options) {
  return new Promise(resolve => {
    let wrapper = function(event) {
      target.removeEventListener(type, wrapper);
      resolve(event);
    };
    target.addEventListener(type, wrapper, options);
  });
}



// Helper function to assert that events are fired and a promise resolved
// in the correct order.
// 'event' should be passed as |should_be_first| to indicate that the events
// should be fired first, otherwise 'promiseresolved' should be passed.
// Attaches |num_listeners| |event| listeners to |object|. If all events have
// been fired and the promise resolved in the correct order, returns a promise
// that fulfills with the result of |object|.|func()| and |event.target.value|
// of each of event listeners. Otherwise throws an error.
function assert_promise_event_order_(should_be_first, object, func, event, num_listeners) {
  let order = [];
  let event_promises = []
  for (let i = 0; i < num_listeners; i++) {
    event_promises.push(new Promise(resolve => {
      let event_listener = (e) => {
        object.removeEventListener(event, event_listener);
        order.push('event');
        resolve(e.target.value);
      }
      object.addEventListener(event, event_listener);
    }));
  }

  let func_promise = object[func]().then(result => {
    order.push('promiseresolved');
    return result;
  });

  return Promise.all([func_promise, ...event_promises])
    .then((result) => {
      if (should_be_first !== order[0]) {
        throw should_be_first === 'promiseresolved' ?
                      `'${event}' was fired before promise resolved.` :
                      `Promise resolved before '${event}' was fired.`;
      }

      if (order[0] !== 'promiseresolved' &&
          order[order.length - 1] !== 'promiseresolved') {
        throw 'Promise resolved in between event listeners.';
      }

      return result;
    });
}

// See assert_promise_event_order_ above.
function assert_promise_resolves_before_event(
  object, func, event, num_listeners=1) {
  return assert_promise_event_order_(
    'promiseresolved', object, func, event, num_listeners);
}

// See assert_promise_event_order_ above.
function assert_promise_resolves_after_event(
  object, func, event, num_listeners=1) {
  return assert_promise_event_order_(
    'event', object, func, event, num_listeners);
}

// Returns a promise that resolves after 100ms unless
// the the event is fired on the object in which case
// the promise rejects.
function assert_no_events(object, event_name) {
  return new Promise((resolve, reject) => {
    let event_listener = (e) => {
      object.removeEventListener(event_name, event_listener);
      assert_unreached('Object should not fire an event.');
    };
    object.addEventListener(event_name, event_listener);
    // TODO(ortuno): Remove timeout.
    // http://crbug.com/543884
    setTimeout(() => {
      object.removeEventListener(event_name, event_listener);
      resolve();
    }, 100);
  });
}

class TestCharacteristicProperties {
  constructor(properties) {
    this.broadcast = properties.broadcast || false;
    this.read = properties.read || false;
    this.writeWithoutResponse = properties.writeWithoutResponse || false;
    this.write = properties.write || false;
    this.notify = properties.notify || false;
    this.indicate = properties.indicate || false;
    this.authenticatedSignedWrites = properties.authenticatedSignedWrites || false;
    this.reliableWrite = properties.reliableWrite || false;
    this.writableAuxiliaries = properties.writableAuxiliaries || false;
  }
}

function assert_properties_equal(properties, expected_properties) {
  for (let key in expected_properties) {
    assert_equals(properties[key], expected_properties[key]);
  }
}

class EventCatcher {
  constructor(object, event) {
    this.eventFired = false;
    let event_listener = () => {
      object.removeEventListener(event, event_listener);
      this.eventFired = true;
    }
    object.addEventListener(event, event_listener);
  }
}

// Returns a function that when called returns a promise that resolves when
// the device has disconnected. Example:
// device.gatt.connect()
//   .then(gatt => get_request_disconnection(gatt))
//   .then(requestDisconnection => requestDisconnection())
//   .then(() => // device is now disconnected)
function get_request_disconnection(gattServer) {
  return gattServer.getPrimaryService(request_disconnection_service_uuid)
    .then(service => service.getCharacteristic(request_disconnection_characteristic_uuid))
    .then(characteristic => {
      return () => assert_promise_rejects_with_message(
        characteristic.writeValue(new Uint8Array([0])),
        new DOMException(
          'GATT Server is disconnected. Cannot perform GATT operations. ' +
          '(Re)connect first with `device.gatt.connect`.',
          'NetworkError'));
    });
}

function generateRequestDeviceArgsWithServices(services = ['heart_rate']) {
  return [{
    filters: [{ services: services }]
  }, {
    filters: [{ services: services, name: 'Name' }]
  }, {
    filters: [{ services: services, namePrefix: 'Pre' }]
  }, {
    filters: [{ services: services, name: 'Name', namePrefix: 'Pre' }]
  }, {
    filters: [{ services: services }],
    optionalServices: ['heart_rate']
  }, {
    filters: [{ services: services, name: 'Name' }],
    optionalServices: ['heart_rate']
  }, {
    filters: [{ services: services, namePrefix: 'Pre' }],
    optionalServices: ['heart_rate']
  }, {
    filters: [{ services: services, name: 'Name', namePrefix: 'Pre' }],
    optionalServices: ['heart_rate']
  }];
}

function setUpPreconnectedDevice({
  address = '00:00:00:00:00:00', name = 'LE Device', knownServiceUUIDs = []}) {
  return navigator.bluetooth.test.simulateCentral({state: 'powered-on'})
    .then(fake_central => fake_central.simulatePreconnectedPeripheral({
      address: address,
      name: name,
      knownServiceUUIDs: knownServiceUUIDs,
    }));
}

function setUpHealthThermometerAndHeartRateDevices() {
  return navigator.bluetooth.test.simulateCentral({state: 'powered-on'})
   .then(fake_central => Promise.all([
     fake_central.simulatePreconnectedPeripheral({
       address: '09:09:09:09:09:09',
       name: 'Health Thermometer',
       knownServiceUUIDs: ['generic_access', 'health_thermometer'],
     }),
     fake_central.simulatePreconnectedPeripheral({
       address: '08:08:08:08:08:08',
       name: 'Heart Rate',
       knownServiceUUIDs: ['generic_access', 'heart_rate'],
     })]));
}
