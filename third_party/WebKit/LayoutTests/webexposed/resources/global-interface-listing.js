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

// FIXME: List interfaces with NoInterfaceObject specified in their IDL file.
debug('[INTERFACES]')
var interfaceNames = Object.getOwnPropertyNames(this).filter(isConstructor);
interfaceNames.sort();
interfaceNames.forEach(function(interfaceName) {
    debug('interface ' + interfaceName);
    var propertyStrings = [];
    var prototype = this[interfaceName].prototype;
    Object.getOwnPropertyNames(prototype).forEach(function(propertyName) {
        var descriptor = Object.getOwnPropertyDescriptor(prototype, propertyName);
        if ('value' in descriptor) {
            var type = typeof descriptor.value === 'function' ? 'method' : 'attribute';
            propertyStrings.push('    ' + type + ' ' + propertyName);
        } else {
            if (descriptor.get)
                propertyStrings.push('    getter ' + propertyName);
            if (descriptor.set)
                propertyStrings.push('    setter ' + propertyName);
        }
    });
    propertyStrings.sort().forEach(debug);
});

if (isWorker())
    finishJSTest();
