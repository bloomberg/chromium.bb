var generic_access = {
  alias: 0x1800,
  name: "generic_access",
  uuid: "00001800-0000-1000-8000-00805f9b34fb",
  device_name: {
    alias: 0x2a00,
    name: "device_name",
    uuid: "00002a00-0000-1000-8000-00805f9b34fb"
  },
  reconnection_address: {
    alias: 0x2a03,
    name: "reconnection_address",
    uuid: "00002a03-0000-1000-8000-00805f9b34fb"
  }
};
var generic_attribute = {
  alias: 0x1801,
  name: "generic_attribute",
  uuid: "00001801-0000-1000-8000-00805f9b34fb"
};
var heart_rate = {
  alias: 0x180d,
  name: "heart_rate",
  uuid: "0000180d-0000-1000-8000-00805f9b34fb"
};
var glucose = {
  alias: 0x1808,
  name: "glucose",
  uuid: "00001808-0000-1000-8000-00805f9b34fb"
};
var battery_service = {
  alias: 0x180f,
  name: "battery_service",
  uuid: "0000180f-0000-1000-8000-00805f9b34fb",
  battery_level: {
    alias: 0x2A19,
    name: "battery_level",
    uuid: "00002a19-0000-1000-8000-00805f9b34fb"
  }
};

// TODO(jyasskin): Upstream this to testharness.js: https://crbug.com/509058.
function callWithKeyDown(functionCalledOnKeyPress) {
    "use strict";
    return new Promise(resolve => {
        function onKeyPress() {
            document.removeEventListener("keypress", onKeyPress, false);
            resolve(functionCalledOnKeyPress());
        }
        document.addEventListener("keypress", onKeyPress, false);

        eventSender.keyDown(" ", []);
    });
}

// Calls requestDevice() in a context that's "allowed to show a popup".
function requestDeviceWithKeyDown() {
    "use strict";
    let args = arguments;
    return callWithKeyDown(() => navigator.bluetooth.requestDevice.apply(navigator.bluetooth, args));
}

// errorUUID(alias) returns a UUID with the top 32 bits of
// "00000000-97e5-4cd7-b9f1-f5a427670c59" replaced with the bits of |alias|.
// For example, errorUUID(0xDEADBEEF) returns
// "deadbeef-97e5-4cd7-b9f1-f5a427670c59". The bottom 96 bits of error UUIDs
// were generated as a type 4 (random) UUID.
function errorUUID(uuidAlias) {
  // Make the number positive.
  uuidAlias >>>= 0;
  // Append the alias as a hex number.
  var strAlias = "0000000" + uuidAlias.toString(16);
  // Get last 8 digits of strAlias.
  strAlias = strAlias.substr(-8);
  // Append Base Error UUID
  return strAlias + "-97e5-4cd7-b9f1-f5a427670c59";
}
