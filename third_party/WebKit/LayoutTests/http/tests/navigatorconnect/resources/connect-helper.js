if ("importScripts" in self) {
  importScripts("/resources/testharness-helpers.js");
  importScripts("test-helpers.js");
}

self.onmessage = function(e) {
  var service = e.data.connect;
  first_to_resolve([wrap_in_port(navigator.services.connect(service)), navigator.connect(service)])
    .then(function(port) {
        e.data.port.postMessage({success: true, result: port}, [port]);
      })
    .catch(function(error) {
        // Not all errors can be serialized as a SerializedScriptValue, so
        // convert to JSON and parse to get just the bits that certainly can.
        e.data.port.postMessage(
          {success: false, result: JSON.parse(stringifyDOMObject(error))});
      });
};
