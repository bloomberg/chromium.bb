'use strict';

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

// Calls testRunner.getBluetoothManualChooserEvents() until it's returned
// |expected_count| events. Or just once if |expected_count| is undefined.
function getBluetoothManualChooserEvents(expected_count) {
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
    assert_true(!!match, event + "isn't an add-device event: " + description);
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
  return Promise.resolve();
}
