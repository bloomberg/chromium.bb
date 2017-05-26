// Avoid polluting the global scope.
(function(globalObject) {

  // Save the list of property names of the global object before loading other scripts.
  var propertyNamesInGlobal = Object.getOwnPropertyNames(globalObject);

  importScripts('../../resources/js-test.js');
  importScripts('../../resources/global-interface-listing.js');

  if (!self.postMessage) {
    // Shared worker.  Make postMessage send to the newest client, which in
    // our tests is the only client.

    // Store messages for sending until we have somewhere to send them.
    self.postMessage = function(message) {
      if (typeof self.pendingMessages === "undefined")
        self.pendingMessages = [];
      self.pendingMessages.push(message);
    };
    self.onconnect = function(event) {
      self.postMessage = function(message) {
        event.ports[0].postMessage(message);
      };
      // Offload any stored messages now that someone has connected to us.
      if (typeof self.pendingMessages === "undefined")
        return;
      while (self.pendingMessages.length)
        event.ports[0].postMessage(self.pendingMessages.shift());
    };
  }

  globalInterfaceListing(globalObject, propertyNamesInGlobal, debug);

  finishJSTest();

})(this);
