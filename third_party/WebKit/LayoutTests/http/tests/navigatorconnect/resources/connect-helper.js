self.onmessage = function(e) {
  navigator.connect(e.data.connect)
    .then(function(port) {
        e.data.port.postMessage({success: true, result: port}, [port]);
      })
    .catch(function(error) {
        // Not all errors can be serialized as a SerializedScriptValue, so
        // convert to JSON and parse to get just the bits that certainly can.
        e.data.port.postMessage(
          {success: false, result: JSON.parse(JSON.stringify(error))});
      });
};
