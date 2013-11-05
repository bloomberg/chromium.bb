if (self.importScripts) {
    importScripts('../../resources/js-test.js');

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
}

function isConstructor(propertyName) {
    var descriptor = Object.getOwnPropertyDescriptor(this, propertyName);
    if (descriptor.value == undefined || descriptor.value.prototype == undefined)
        return false;
    return descriptor.writable && !descriptor.enumerable && descriptor.configurable;
}

var constructorNames = [];
var propertyNames = Object.getOwnPropertyNames(this);
for (var i = 0; i < propertyNames.length; i++) {
    if (isConstructor(propertyNames[i]))
        constructorNames[constructorNames.length] = propertyNames[i];
}


constructorNames.sort();
for (var i = 0; i < constructorNames.length; i++)
    debug(constructorNames[i]);

if (isWorker())
  finishJSTest();
