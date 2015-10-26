// Run all the code in a local scope.
(function(globalObject) {

// Save the list of property names of the global object before loading other scripts.
var propertyNamesInGlobal = globalObject.propertyNamesInGlobal || Object.getOwnPropertyNames(globalObject);

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

// List of builtin JS constructors; Blink is not controlling what properties these
// objects have, so exercising them in a Blink test doesn't make sense.
//
// If new builtins are added, please update this list along with the one in
// LayoutTests/http/tests/serviceworker/webexposed/resources/global-interface-listing-worker.js
var jsBuiltins = new Set([
    'Array',
    'ArrayBuffer',
    'Boolean',
    'Date',
    'Error',
    'EvalError',
    'Float32Array',
    'Float64Array',
    'Function',
    'Infinity',
    'Int16Array',
    'Int32Array',
    'Int8Array',
    'Intl',
    'JSON',
    'Map',
    'Math',
    'NaN',
    'Number',
    'Object',
    'Promise',
    'RangeError',
    'ReferenceError',
    'RegExp',
    'Set',
    'String',
    'Symbol',
    'SyntaxError',
    'TypeError',
    'URIError',
    'Uint16Array',
    'Uint32Array',
    'Uint8Array',
    'Uint8ClampedArray',
    'WeakMap',
    'WeakSet',
    'decodeURI',
    'decodeURIComponent',
    'encodeURI',
    'encodeURIComponent',
    'escape',
    'eval',
    'isFinite',
    'isNaN',
    'parseFloat',
    'parseInt',
    'undefined',
    'unescape',
]);

function isWebIDLConstructor(propertyName) {
    if (jsBuiltins.has(propertyName))
        return false;
    var descriptor = Object.getOwnPropertyDescriptor(this, propertyName);
    if (descriptor.value == undefined || descriptor.value.prototype == undefined)
        return false;
    return descriptor.writable && !descriptor.enumerable && descriptor.configurable;
}

function collectPropertyInfo(object, propertyName, output) {
    var descriptor = Object.getOwnPropertyDescriptor(object, propertyName);
    if ('value' in descriptor) {
        var type = typeof descriptor.value === 'function' ? 'method' : 'attribute';
        output.push('    ' + type + ' ' + propertyName);
    } else {
        if (descriptor.get)
            output.push('    getter ' + propertyName);
        if (descriptor.set)
            output.push('    setter ' + propertyName);
    }
}

// FIXME: List interfaces with NoInterfaceObject specified in their IDL file.
debug('[INTERFACES]');
var interfaceNames = Object.getOwnPropertyNames(this).filter(isWebIDLConstructor);
interfaceNames.sort();
interfaceNames.forEach(function(interfaceName) {
    var inheritsFrom = this[interfaceName].__proto__.name;
    if (inheritsFrom)
        debug('interface ' + interfaceName + ' : ' + inheritsFrom);
    else
        debug('interface ' + interfaceName);
    var propertyStrings = [];
    var prototype = this[interfaceName].prototype;
    Object.getOwnPropertyNames(prototype).forEach(function(propertyName) {
        collectPropertyInfo(prototype, propertyName, propertyStrings);
    });
    propertyStrings.sort().forEach(debug);
});

debug('[GLOBAL OBJECT]');
var propertyStrings = [];
var memberNames = propertyNamesInGlobal.filter(function(propertyName) {
    return !jsBuiltins.has(propertyName) && !isWebIDLConstructor(propertyName);
});
memberNames.forEach(function(propertyName) {
    collectPropertyInfo(globalObject, propertyName, propertyStrings);
});
propertyStrings.sort().forEach(debug);

if (isWorker())
    finishJSTest();

})(this); // Run all the code in a local scope.
